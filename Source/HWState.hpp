#ifndef GBX_HWSTATE_HPP_
#define GBX_HWSTATE_HPP_
#include <Utix/Ints.h>

namespace gbx {


enum IntMask : uint8_t
{
	INT_VBLANK = 0x01,
	INT_LCD_STAT = 0x02,
	INT_TIMER = 0x04,
	INT_SERIAL = 0x08,
	INT_JOYPAD = 0x10
};



struct HWState
{

	enum Flags : uint8_t
	{
		INTERRUPT_MASTER_ENABLED = 0x01,
		INTERRUPT_MASTER_ACTIVE = 0x02,
		CPU_HALT = 0x04,
		TIMER_STOP = 0x08
	};


	bool GetIntMaster() const;
	bool GetIntActive() const;
	Flags GetFlags(Flags hwflags) const;
	uint8_t GetPendentInts() const;
	void EnableIntMaster();
	void EnableIntActive();
	void DisableIntMaster();

	void SetFlags(Flags hwflags);
	void ClearFlags(Flags hwflags);

	void EnableInt(const IntMask intr);
	void DisableInt(const IntMask inter);
	void RequestInt(const IntMask inter);
	void ClearInt(const IntMask inter);

	uint16_t div_clock;
	uint16_t tima_clock;
	uint16_t tima_clock_limit;
	uint8_t flags;
	uint8_t div;
	uint8_t tima;
	uint8_t tma;
	uint8_t tac;
	uint8_t interrupt_enable;
	uint8_t interrupt_flags;
};




inline bool HWState::GetIntMaster() const 
{
	return (flags & INTERRUPT_MASTER_ENABLED) != 0;
}

inline bool HWState::GetIntActive() const 
{
	return (flags & INTERRUPT_MASTER_ACTIVE) != 0;
}


inline HWState::Flags HWState::GetFlags(Flags hwflags) const
{
	return static_cast<Flags>(flags & hwflags);
}


inline uint8_t HWState::GetPendentInts() const
{
	return 0x1f & interrupt_enable & interrupt_flags;
}






inline void HWState::EnableIntMaster() 
{
	flags |= INTERRUPT_MASTER_ENABLED;
}

inline void HWState::EnableIntActive()
{
	flags |= INTERRUPT_MASTER_ACTIVE;
}

inline void HWState::DisableIntMaster() 
{
	flags &= ~(INTERRUPT_MASTER_ENABLED 
	           | INTERRUPT_MASTER_ACTIVE);
}


inline void HWState::SetFlags(Flags hwflags)
{
	flags |= hwflags;
}

inline void HWState::ClearFlags(Flags hwflags)
{
	flags &= ~hwflags;
}



inline void HWState::EnableInt(const IntMask inter)
{
	interrupt_enable |= inter;
}

inline void HWState::DisableInt(const IntMask inter)
{
	interrupt_enable &= ~inter;
}

inline void HWState::RequestInt(const IntMask inter)
{
	interrupt_flags |= inter;
}


inline void HWState::ClearInt(const IntMask inter)
{
	interrupt_flags &= ~inter;
}














}
#endif
