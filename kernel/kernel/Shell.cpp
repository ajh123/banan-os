#include <BAN/Math.h>
#include <BAN/ScopeGuard.h>
#include <BAN/StringView.h>
#include <BAN/Vector.h>
#include <kernel/CPUID.h>
#include <kernel/Input.h>
#include <kernel/IO.h>
#include <kernel/PIT.h>
#include <kernel/PCI.h>
#include <kernel/Process.h>
#include <kernel/RTC.h>
#include <kernel/Shell.h>

#include <fcntl.h>
#include <ctype.h>

#define TTY_PRINT(...) BAN::Formatter::print([this](char c) { m_tty->putchar(c); }, __VA_ARGS__)
#define TTY_PRINTLN(...) BAN::Formatter::println([this](char c) { m_tty->putchar(c); }, __VA_ARGS__)

namespace Kernel
{

	static auto s_default_prompt = "\\[\e[32m\\]user\\[\e[m\\]:\\[\e[34m\\]\\w\\[\e[m\\]# "sv;

	Shell::Shell(TTY* tty)
		: m_tty(tty)
	{
		Input::register_key_event_callback({ &Shell::key_event_callback, this });
		MUST(set_prompt(s_default_prompt));
		MUST(m_buffer.push_back(""sv));
	}

	BAN::ErrorOr<void> Shell::set_prompt(BAN::StringView prompt)
	{
		m_prompt_string = prompt;
		TRY(update_prompt());
		return {};
	}

	BAN::ErrorOr<void> Shell::update_prompt()
	{
		m_prompt_length = 0;
		m_prompt.clear();

		bool skipping = false;
		for (size_t i = 0; i < m_prompt_string.size(); i++)
		{
			if (i < m_prompt_string.size() - 1 && m_prompt_string[i] == '\\')
			{
				switch (m_prompt_string[i + 1])
				{
					case '[':
						skipping = true;
						break;
					case ']':
						skipping = false;
						break;
					case 'w':
					{
						auto working_directory = Process::current()->working_directory();
						TRY(m_prompt.append(working_directory));
						m_prompt_length += working_directory.size();
						break;
					}
					default:
						dprintln("unknown escape character '{}' in shell prompt", m_prompt_string[i + 1]);
						break;
				}
				i++;
				continue;
			}
			
			TRY(m_prompt.push_back(m_prompt_string[i]));
			if (!skipping)
				m_prompt_length++;
		}

		return {};
	}

	void Shell::run()
	{
		TTY_PRINT("{}", m_prompt);
		for (;;)
		{
			PIT::sleep(1); // sleep until next reschedule
			Input::update();
		}
	}

	BAN::Vector<BAN::String> Shell::parse_arguments(BAN::StringView command) const
	{
		BAN::Vector<BAN::String> result;

		while (!command.empty())
		{
			while (!command.empty() && isspace(command.front()))
				command = command.substring(1);
			
			if (command.empty())
				break;

			MUST(result.push_back(""sv));

			char quoted = '\0';
			bool escape = false;
			while (!command.empty())
			{
				char ch = command.front();
				switch (ch)
				{
					case '"':
					case '\'':
						if (!quoted)
							quoted = ch;
						else if (ch == quoted)
							quoted = '\0';
						else
							goto default_case;
						break;
					case '\\':
						if (escape)
							goto default_case;
						escape = true;
						break;
					default:
default_case:
						if (isspace(ch) && !quoted && !escape)
							goto argument_done;
						if (quoted && escape)
						{
							switch (ch)
							{
								case 'f':  MUST(result.back().push_back('\f')); break;
								case 'n':  MUST(result.back().push_back('\n')); break;
								case 'r':  MUST(result.back().push_back('\r')); break;
								case 't':  MUST(result.back().push_back('\t')); break;
								case 'v':  MUST(result.back().push_back('\v')); break;
								case '"':  MUST(result.back().push_back('"'));  break;
								case '\'': MUST(result.back().push_back('\'')); break;
								case '\\': MUST(result.back().push_back('\\')); break;
								default:
									char buffer[3] { '\\', ch, '\0' };
									MUST(result.back().append(buffer));
									break;
							}
						}
						else
						{
							MUST(result.back().push_back(ch));
						}
						escape = false;
						break;
				}
				command = command.substring(1);
			}
argument_done:
			continue;
		}

		return result;
	}

	extern uint32_t crc32_table[256];

	BAN::ErrorOr<void> Shell::process_command(const BAN::Vector<BAN::String>& arguments)
	{
		if (arguments.empty())
		{

		}
		else if (arguments.front() == "date")
		{
			if (arguments.size() != 1)
				return BAN::Error::from_c_string("'date' does not support command line arguments");
			auto time = RTC::get_current_time();
			TTY_PRINTLN("{}", time);
		}
		else if (arguments.front() == "echo")
		{
			if (arguments.size() > 1)
			{
				TTY_PRINT("{}", arguments[1]);
				for (size_t i = 2; i < arguments.size(); i++)
					TTY_PRINT(" {}", arguments[i]);
			}
			TTY_PRINTLN("");
		}
		else if (arguments.front() == "clear")
		{
			if (arguments.size() != 1)
				return BAN::Error::from_c_string("'clear' does not support command line arguments");
			m_tty->clear();
			m_tty->set_cursor_position(0, 0);
		}
		else if (arguments.front() == "time")
		{
			auto new_args = arguments;
			new_args.remove(0);
			auto start = PIT::ms_since_boot();
			TRY(process_command(new_args));
			auto duration = PIT::ms_since_boot() - start;
			TTY_PRINTLN("took {} ms", duration);
		}
		else if (arguments.front() == "thread")
		{
			struct thread_data_t
			{
				Shell* shell;
				SpinLock& lock;
				const BAN::Vector<BAN::String>& arguments;
			};
			
			auto function = [](void* data)
			{
				thread_data_t* thread_data = (thread_data_t*)data;
				Shell* shell = thread_data->shell;
				auto args = thread_data->arguments;
				thread_data->lock.unlock();

				args.remove(0);
				PIT::sleep(5000);

				if (auto res = shell->process_command(args); res.is_error())
					BAN::Formatter::println([&](char c) { shell->m_tty->putchar(c); }, "{}", res.error());
			};

			SpinLock spinlock;
			thread_data_t thread_data = { this, spinlock, arguments };
			spinlock.lock();
			TRY(Process::current()->add_thread(function, &thread_data));
			while (spinlock.is_locked());
		}
		else if (arguments.front() == "memory")
		{
			if (arguments.size() != 1)
				return BAN::Error::from_c_string("'memory' does not support command line arguments");
			kmalloc_dump_info();
		}
		else if (arguments.front() == "sleep")
		{
			if (arguments.size() != 1)
				return BAN::Error::from_c_string("'sleep' does not support command line arguments");
			PIT::sleep(5000);
		}
		else if (arguments.front() == "cpuinfo")
		{
			if (arguments.size() != 1)
				return BAN::Error::from_c_string("'cpuinfo' does not support command line arguments");

			uint32_t ecx, edx;
			auto vendor = CPUID::get_vendor();
			CPUID::get_features(ecx, edx);

			TTY_PRINTLN("Vendor: '{}'", vendor);
			TTY_PRINTLN("64-bit: {}", CPUID::is_64_bit());
			bool first = true;
			for (int i = 0; i < 32; i++)
				if (ecx & ((uint32_t)1 << i))
					TTY_PRINT("{}{}", first ? (first = false, "") : ", ", CPUID::feature_string_ecx((uint32_t)1 << i));
			for (int i = 0; i < 32; i++)
				if (edx & ((uint32_t)1 << i))
					TTY_PRINT("{}{}", first ? (first = false, "") : ", ", CPUID::feature_string_edx((uint32_t)1 << i));
			if (!first)
				TTY_PRINTLN("");
		}
		else if (arguments.front() == "random")
		{
			if (arguments.size() != 1)
				return BAN::Error::from_c_string("'random' does not support command line arguments");
			uint32_t ecx, edx;
			CPUID::get_features(ecx, edx);
			if (!(ecx & CPUID::Features::ECX_RDRND))
				return BAN::Error::from_c_string("cpu does not support RDRAND instruction");

			for (int i = 0; i < 10; i++)
			{
				uint32_t random;
				asm volatile("rdrand %0" : "=r"(random));
				TTY_PRINTLN("  0x{8H}", random);
			}
		}
		else if (arguments.front() == "reboot")
		{
			if (arguments.size() != 1)
				return BAN::Error::from_c_string("'reboot' does not support command line arguments");
			uint8_t good = 0x02;
			while (good & 0x02)
				good = IO::inb(0x64);
			IO::outb(0x64, 0xFE);
			asm volatile("cli; hlt");
		}
		else if (arguments.front() == "lspci")
		{
			if (arguments.size() != 1)
				return BAN::Error::from_c_string("'lspci' does not support command line arguments");
			for (auto& device : PCI::get().devices())
				TTY_PRINTLN("{2H}:{2H}.{2H} {2H}", device.bus(), device.dev(), device.func(), device.class_code());
		}
		else if (arguments.front() == "ls")
		{
			if (arguments.size() > 2)
				return BAN::Error::from_c_string("usage: 'ls [path]'");

			BAN::StringView path = (arguments.size() == 2) ? arguments[1].sv() : Process::current()->working_directory();

			int fd = TRY(Process::current()->open(path, O_RDONLY));
			BAN::ScopeGuard _([fd] { MUST(Process::current()->close(fd)); });

			auto& directory = Process::current()->inode_for_fd(fd);
			auto inodes = TRY(directory.directory_inodes());

			auto mode_string = [](Inode::Mode mode)
			{
				static char buffer[11] {};
				buffer[0] = mode.IFDIR ? 'd' : '-';
				buffer[1] = mode.IRUSR ? 'r' : '-';
				buffer[2] = mode.IWUSR ? 'w' : '-';
				buffer[3] = mode.IXUSR ? 'x' : '-';
				buffer[4] = mode.IRGRP ? 'r' : '-';
				buffer[5] = mode.IWGRP ? 'w' : '-';
				buffer[6] = mode.IXGRP ? 'x' : '-';
				buffer[7] = mode.IROTH ? 'r' : '-';
				buffer[8] = mode.IWOTH ? 'w' : '-';
				buffer[9] = mode.IXOTH ? 'x' : '-';
				return (const char*)buffer;
			};

			for (auto& inode : inodes)
				if (inode->ifdir())
					TTY_PRINTLN("  {} {7} \e[34m{}\e[m", mode_string(inode->mode()), inode->size(), inode->name());
			for (auto& inode : inodes)
				if (!inode->ifdir())
					TTY_PRINTLN("  {} {7} {}", mode_string(inode->mode()), inode->size(), inode->name());
		}
		else if (arguments.front() == "cat")
		{
			if (arguments.size() != 2)
				return BAN::Error::from_c_string("usage: 'cat path'");
			
			int fd = TRY(Process::current()->open(arguments[1], O_RDONLY));
			BAN::ScopeGuard _([fd] { MUST(Process::current()->close(fd)); });

			char buffer[1024] {};
			while (true)
			{
				size_t n_read = TRY(Process::current()->read(fd, buffer, sizeof(buffer)));
				if (n_read == 0)
					break;
				TTY_PRINT("{}", BAN::StringView(buffer, n_read));
			}
			TTY_PRINTLN("");
		}
		else if (arguments.front() == "cd")
		{
			if (arguments.size() > 2)
				return BAN::Error::from_c_string("usage 'cd path'");
			BAN::StringView path = arguments.size() == 2 ? arguments[1].sv() : "/"sv;
			TRY(Process::current()->set_working_directory(path));
			TRY(update_prompt());
		}
		else if (arguments.front() == "cksum")
		{
			if (arguments.size() < 2)
				return BAN::Error::from_c_string("usage 'cksum paths...'");

			uint8_t buffer[1024];
			for (size_t i = 1; i < arguments.size(); i++)
			{
				int fd = TRY(Process::current()->open(arguments[i], O_RDONLY));
				BAN::ScopeGuard _([fd] { MUST(Process::current()->close(fd)); });

				uint32_t crc32 = 0;
				uint32_t total_read = 0;

				while (true)
				{
					size_t n_read = TRY(Process::current()->read(fd, buffer, sizeof(buffer)));
					if (n_read == 0)
						break;
					for (size_t j = 0; j < n_read; j++)
        				crc32 = (crc32 << 8) ^ crc32_table[((crc32 >> 24) ^ buffer[j]) & 0xFF];
					total_read += n_read;
				}

				for (uint32_t length = total_read; length; length >>= 8)
					crc32 = (crc32 << 8) ^ crc32_table[((crc32 >> 24) ^ length) & 0xFF];
				crc32 = ~crc32 & 0xFFFFFFFF;

				TTY_PRINTLN("{} {} {}", crc32, total_read, arguments[i]);
			}
		}
		else if (arguments.front() == "loadfont")
		{
			if (arguments.size() != 2)
				return BAN::Error::from_c_string("usage: 'loadfont font_path'");

			auto font = TRY(Font::load(arguments[1]));
			m_tty->set_font(font);
		}
		else
		{
			return BAN::Error::from_format("unrecognized command '{}'", arguments.front());
		}

		return {};
	}

	void Shell::rerender_buffer() const
	{
		TTY_PRINT("\e[{}G{}\e[K", m_prompt_length + 1, m_buffer[m_cursor_pos.line]);
	}

	static uint32_t get_last_length(BAN::StringView sv)
	{
		if (sv.size() >= 2 && ((uint8_t)sv[sv.size() - 2] >> 5) == 0b110)	return 2;
		if (sv.size() >= 3 && ((uint8_t)sv[sv.size() - 3] >> 4) == 0b1110)	return 3;
		if (sv.size() >= 4 && ((uint8_t)sv[sv.size() - 4] >> 3) == 0b11110)	return 4;
		return BAN::Math::min<uint32_t>(sv.size(), 1);
	}

	static uint32_t get_next_length(BAN::StringView sv)
	{
		if (sv.size() >= 2 && ((uint8_t)sv[0] >> 5) == 0b110)	return 2;
		if (sv.size() >= 3 && ((uint8_t)sv[0] >> 4) == 0b1110)	return 3;
		if (sv.size() >= 4 && ((uint8_t)sv[0] >> 3) == 0b11110)	return 4;
		return BAN::Math::min<uint32_t>(sv.size(), 1);
	}

	static uint32_t get_unicode_character_count(BAN::StringView sv)
	{
		uint32_t len = 0;
		for (uint32_t i = 0; i < sv.size(); i++)
		{
			uint8_t ch = sv[i];
			if ((ch >> 5) == 0b110)		i += 1;
			if ((ch >> 4) == 0b1110)	i += 2;
			if ((ch >> 3) == 0b11110)	i += 3;
			len++;
		}
		return len;
	}

	void Shell::key_event_callback(Input::KeyEvent event)
	{
		if (!event.pressed)
			return;

		BAN::String& current_buffer = m_buffer[m_cursor_pos.line];

		switch (event.key)
		{
			case Input::Key::Backspace:
				if (m_cursor_pos.col > 0)
				{
					TTY_PRINT("\e[D{} ", current_buffer.sv().substring(m_cursor_pos.index));
					
					uint32_t len = get_last_length(current_buffer.sv().substring(0, m_cursor_pos.index));
					m_cursor_pos.index -= len;
					current_buffer.erase(m_cursor_pos.index, len);
					m_cursor_pos.col--;
				}
				break;

			case Input::Key::Enter:
			case Input::Key::NumpadEnter:
			{
				TTY_PRINTLN("");
				auto arguments = parse_arguments(current_buffer.sv());
				if (!arguments.empty())
				{
					if (auto res = process_command(arguments); res.is_error())
						TTY_PRINTLN("{}", res.error());
					MUST(m_old_buffer.push_back(current_buffer));
					m_buffer = m_old_buffer;
					MUST(m_buffer.push_back(""sv));
					m_cursor_pos.line = m_buffer.size() - 1;
				}
				m_cursor_pos.col = 0;
				m_cursor_pos.index = 0;
				TTY_PRINT("{}", m_prompt);
				break;
			}

			case Input::Key::Escape:
				TTY_PRINTLN("time since boot {} ms", PIT::ms_since_boot());
				break;

			case Input::Key::Tab:
				break;
			
			case Input::Key::Left:
				if (m_cursor_pos.index > 0)
				{					
					uint32_t len = get_last_length(current_buffer.sv().substring(0, m_cursor_pos.index));
					m_cursor_pos.index -= len;
					m_cursor_pos.col--;
				}
				break;

			case Input::Key::Right:
				if (m_cursor_pos.index < current_buffer.size())
				{
					uint32_t len = get_next_length(current_buffer.sv().substring(m_cursor_pos.index));
					m_cursor_pos.index += len;
					m_cursor_pos.col++;
				}
				break;

			case Input::Key::Up:
				if (m_cursor_pos.line > 0)
				{
					const auto& new_buffer = m_buffer[m_cursor_pos.line - 1];
					m_cursor_pos.line--;
					m_cursor_pos.index = new_buffer.size();
					m_cursor_pos.col = get_unicode_character_count(new_buffer);
					rerender_buffer();
				}
				break;

			case Input::Key::Down:
				if (m_cursor_pos.line < m_buffer.size() - 1)
				{
					const auto& new_buffer = m_buffer[m_cursor_pos.line + 1];
					m_cursor_pos.line++;
					m_cursor_pos.index = new_buffer.size();
					m_cursor_pos.col = get_unicode_character_count(new_buffer);
					rerender_buffer();
				}
				break;

			case Input::Key::A:
				if (event.modifiers & 2)
				{
					m_cursor_pos.col = m_cursor_pos.index = 0;
					break;
				}
				// fall through

			default:
			{
				const char* utf8 = Input::key_event_to_utf8(event);
				if (utf8)
				{
					TTY_PRINT("{}{}", utf8, current_buffer.sv().substring(m_cursor_pos.index));
					MUST(current_buffer.insert(utf8, m_cursor_pos.index));
					m_cursor_pos.index += strlen(utf8);
					m_cursor_pos.col++;
				}
				break;
			}
		}

		TTY_PRINT("\e[{}G", m_prompt_length + m_cursor_pos.col + 1);
	}

}