## ps4sdk.cmake - Playstation4 cross-compile
#
set(CMAKE_SYSTEM_NAME FreeBSD) # this one is important
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_SYSTEM_VERSION 9)   # this one not so much



set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER) # search for programs in the build host directories
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH)  # for libraries and headers in the target directories
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH)





if (NOT "" STREQUAL "$ENV{OO_PS4_TOOLCHAIN}")
#
	file(TO_CMAKE_PATH $ENV{OO_PS4_TOOLCHAIN} PS4SDK)
#
elseif(NOT "" STREQUAL "$ENV{PS4SDK}")
#
	file(TO_CMAKE_PATH $ENV{PS4SDK} PS4SDK)
#
else()
#
	error("Need PS4SDK set!")
#
endif()




set(triple "x86_64-scei-ps4-elf")

set(CMAKE_CXX_COMPILER_FORCED ON)
	
set(CMAKE_C_COMPILER   clang)
set(CMAKE_CXX_COMPILER clang++)
	
set(CMAKE_C_COMPILER_TARGET ${triple})


set(CMAKE_LINKER "ld.lld")
set(CMAKE_C_LINK_EXECUTABLE "<CMAKE_LINKER> -o <TARGET> <LINK_FLAGS> <OBJECTS> <LINK_LIBRARIES>") # <FLAGS> <CMAKE_C_LINK_FLAGS> ")
set(CMAKE_CXX_LINK_EXECUTABLE "<CMAKE_LINKER> -o <TARGET> <FLAGS> <CMAKE_CXX_LINK_FLAGS> <LINK_FLAGS> <OBJECTS>  <LINK_LIBRARIES>")

set(CMAKE_C_FLAGS_INIT	"-v -target x86_64-scei-ps4 -Xclang -emit-obj -Xclang -munwind-tables")
#set(CMAKE_C_FLAGS	"-target x86_64-scei-ps4")


include_directories(${PS4SDK}/include)
include_directories(${PS4SDK}/include/orbis)

link_directories(${PS4SDK}/lib)


set(OO_LDS "${PS4SDK}/link.x")
set(OO_CRT "${PS4SDK}/lib/crt1.o")

set(OO_LIBS -lc -lkernel)
#link_libraries(-lc -lkernel)	This is adding dir w. -Wl,-rpath even though it's already set with -L ;smdh;

#-m elf_x86_64 -pie --script "%OO_PS4_TOOLCHAIN%\link.x" --eh-frame-hdr -o "%outputElf%" "-L%OO_PS4_TOOLCHAIN%\\lib" %libraries% --verbose "%OO_PS4_TOOLCHAIN%\lib\crt1.o" %obj_files%
add_link_options(-m elf_x86_64 -pie --script "${OO_LDS}" --eh-frame-hdr --verbose "${OO_CRT}" ${OO_LIBS})



#set(CMAKE_EXECUTABLE_SUFFIX_C ".elf")
#set(CMAKE_EXECUTABLE_SUFFIX_CXX ".elf")


set(PS4 ON)
set(ORBIS ON)

add_definitions(-DPS4 -D__Orbis__ -DOO)





