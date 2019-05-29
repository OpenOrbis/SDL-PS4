## ps4sdk.cmake - Playstation4 cross-compile
#
set(CMAKE_SYSTEM_NAME Orbis) # this one is important
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_SYSTEM_VERSION 9)   # this one not so much



### This shit is very WIP ###
#
## TODO: Check for 





#SCE_ORBIS_SDK_DIR=D:\Dev\PS4\SCE\PS4SDK
#SCE_ROOT_DIR=D:\Dev\PS4\SCE


if (NOT "" STREQUAL "$ENV{SCE_ORBIS_SDK_DIR}")

  file(TO_CMAKE_PATH $ENV{SCE_ORBIS_SDK_DIR} SCESDK)
  #set(SCESDK $ENV{SCE_ORBIS_SDK_DIR})	# SCE_ORBIS_SDK_DIR  - requires change below ./SCE/PS4SDK : ./
endif()

  
if (NOT "" STREQUAL "${SCESDK}")		# defaults to sce sdk if available
  set(USE_SCE ON)
  set(PS4SDK ${SCESDK})
endif()



if ("" STREQUAL "${PS4SDK}")			# Not found or passed in

  if(NOT "" STREQUAL "$ENV{PS4SDK}")	# Try env var
    set(PS4SDK $ENV{PS4SDK})
  else()								# else defaults
    if ("Windows" STREQUAL "${CMAKE_HOST_SYSTEM_NAME}")
      set(PS4SDK "D:/Dev/ps4sdk")
    else()
      set(PS4SDK "/opt/ps4/sdk")
    endif()
  endif()

endif()


set(TAUON_SDK ${PS4SDK}/tauon)



if(USE_SCE)
#
	set(PS4HOST   ${PS4SDK}/host_tools)
	set(PS4TARGET ${PS4SDK}/target)

	set(CMAKE_FIND_ROOT_PATH  ${PS4TARGET}) # where is the target environment
	

	set(toolPrefix "orbis-")
	set(toolSuffix ".exe")

	
	file(TO_CMAKE_PATH ${PS4HOST}/bin/${toolPrefix}clang${toolSuffix}   CMAKE_C_COMPILER)
	file(TO_CMAKE_PATH ${PS4HOST}/bin/${toolPrefix}clang++${toolSuffix} CMAKE_CXX_COMPILER)

#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -masm=intel -fms-extensions -fasm-blocks ")

#	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -nobuiltininc ")	
#	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -nostdinc -nostdinc++ ")

#	-nobuiltininc           Disable builtin #include directories
#	-nostdinc++             Disable standard #include directories for the C++ standard library

	
	set (PS4_inc_dirs 
	#	${TAUON_SDK}/include 
		${PS4TARGET}/include 
		${PS4TARGET}/include_common
	)

#	set (PS4_link_dirs
#		"${PS4TARGET}/lib"
#		"${PS4TARGET}/tauon/lib"
#	)

	
#LDFLAGS += -L $(TAUON_SDK_DIR)/lib -L $(SCE_ORBIS_SDK_DIR)/target/lib
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s -Wl,--addressing=non-aslr,--strip-unused-data ")
#	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -L ${TAUON_SDK}/lib")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -L ${PS4TARGET}/lib")

	message("CMAKE_EXE_LINKER_FLAGS ${CMAKE_EXE_LINKER_FLAGS}")
#
else()
#
	set(triple "x86_64-scei-ps4")

	set(CMAKE_C_COMPILER_TARGET ${triple})
	set(CMAKE_CXX_COMPILER_TARGET ${triple})
	
	set(CMAKE_C_COMPILER   clang)
	set(CMAKE_CXX_COMPILER clang++)

	
	set (PS4_inc_dirs 
		${TAUON_SDK}/include 

	#	${PS4SDK}/include 
	#	${PS4SDK}/tauon/include 
	)

#	set (PS4_link_dirs
#		"${PS4SDK}/lib"
#		"${PS4SDK}/tauon/lib"
#	)

	
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s -Wl,--addressing=non-aslr,--strip-unused-data -L${TAUON_SDK}/lib")
#
endif()
### IF NOT SCE SDK
#



set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER) # search for programs in the build host directories
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH)  # for libraries and headers in the target directories
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH)


include_directories(${PS4_inc_dirs})




set(PS4 ON)
set(ORBIS ON)

add_definitions(-DPS4)

if(PS4)
  set(binSuffix ".elf")
    link_libraries(SceSystemService_stub_weak SceUserService_stub_weak ScePosix_stub_weak 
					ScePad_stub_weak SceAudioOut_stub_weak SceSysmodule_stub_weak
                    libc_stub_weak.a
					#${PS4TARGET}/lib/libc_stub_weak.a
					#SceLibcInternal_tau_stub
	#	SceSysmodule_tau_stub_weak 
	#	kernel_tau_stub_weak SceSysmodule_tau_stub_weak SceSystemService_stub_weak SceSystemService_tau_stub_weak SceShellCoreUtil_tau_stub_weak ScePigletv2VSH_tau_stub_weak kernel_util
	)
endif()


#set(LINK_DIRECTORIES ${PS4_link_dirs})
#link_directories(${PS4_link_dirs})








