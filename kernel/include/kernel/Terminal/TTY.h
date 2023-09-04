#pragma once

#include <BAN/Array.h>
#include <kernel/Device/Device.h>
#include <kernel/Input/KeyEvent.h>
#include <kernel/SpinLock.h>
#include <kernel/Terminal/TerminalDriver.h>
#include <kernel/Terminal/termios.h>
#include <kernel/Semaphore.h>

namespace Kernel
{

	class TTY : public CharacterDevice
	{
	public:
		void set_termios(const termios& termios) { m_termios = termios; }
		termios get_termios() const { return m_termios; }	
		virtual void set_font(const Font&) {};

		void set_foreground_pgrp(pid_t pgrp) { m_foreground_pgrp = pgrp; }
		pid_t foreground_pgrp() const { return m_foreground_pgrp; }

		// for kprint
		static void putchar_current(uint8_t ch);
		static bool is_initialized();
		static BAN::RefPtr<TTY> current();
		void set_as_current();

		static void initialize_devices();
		void on_key_event(Input::KeyEvent);
		void handle_input(const uint8_t* ch);

		virtual bool is_tty() const override { return true; }

		virtual BAN::ErrorOr<size_t> read(size_t, void*, size_t) override;
		virtual BAN::ErrorOr<size_t> write(size_t, const void*, size_t) override;

		virtual uint32_t height() const = 0;
		virtual uint32_t width() const = 0;
		virtual void putchar(uint8_t ch) = 0;

		bool has_data() const;

	protected:
		TTY(mode_t mode, uid_t uid, gid_t gid)
			: CharacterDevice(mode, uid, gid)
		{ }

		virtual BAN::StringView name() const = 0;

	private:
		void do_backspace();

	protected:
		mutable Kernel::RecursiveSpinLock m_lock;

		TerminalDriver::Color m_foreground { TerminalColor::BRIGHT_WHITE };
		TerminalDriver::Color m_background { TerminalColor::BLACK };
		termios m_termios;

	private:
		pid_t m_foreground_pgrp { 0 };

		struct Buffer
		{
			BAN::Array<uint8_t, 1024> buffer;
			size_t bytes { 0 };
			bool flush { false };
			Semaphore semaphore;
		};
		Buffer m_output;
	};

}
