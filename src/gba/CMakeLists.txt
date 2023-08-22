cmake_minimum_required (VERSION 3.25)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE
	arm7tdmi/arm.cpp
	arm7tdmi/arm7tdmi.cpp
	arm7tdmi/exceptions.cpp
	arm7tdmi/thumb.cpp

	ppu/interface.cpp
	ppu/ppu.cpp
	ppu/rendering.cpp

	apu.cpp
	bios.cpp
	bus.cpp
	cart.cpp
	debug.cpp
	dma.cpp
	gba.cpp
	irq.cpp
	keypad.cpp
	scheduler.cpp
	serial.cpp
	timers.cpp
)