cmake_minimum_required (VERSION 3.13)

target_sources(teesoe PRIVATE
	main.cpp

	common/log.cpp
	
	frontend/gui.cpp
	frontend/message.cpp
  )

target_include_directories(teesoe PRIVATE
	.
	common
)