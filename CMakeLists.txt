# This CMakeLists has only been tested on my OSX machine.
list(INSERT CMAKE_MODULE_PATH 0 "${CMAKE_SOURCE_DIR}/cmake")
cmake_minimum_required(VERSION 3.6)

project(LLL-TAO)
set(CMAKE_CXX_STANDARD 11)
set(OPENSSL_ROOT_DIR /usr/local/opt/openssl)
add_definitions(-DMAC_OSX -DMSG_NOSIGNAL=0 -DBOOST_SPIRIT_THREADSAFE -DBOOST_NO_CXX11_SCOPED_ENUMS)

find_package(BerkeleyDB REQUIRED)
find_package(LevelDB REQUIRED)
find_package(Boost COMPONENTS filesystem system program_options thread REQUIRED)
find_package(OpenSSL COMPONENTS ssl crypto REQUIRED)

include_directories(${Boost_INCLUDE_DIR})
include_directories(${OPENSSL_INCLUDE_DIR})
include_directories(${LevelDB_INCLUDE})
include_directories(./src/)
include_directories(./src/LLC/)
include_directories(./src/LLC/hash/)
include_directories(./src/LLC/hash/SK/)
include_directories(./src/LLC/hash/Skein3Fish/)
include_directories(./src/LLC/hash/Skein3Fish/include)
include_directories(./src/LLC/include/)
include_directories(./src/LLC/types/)
include_directories(./src/LLD/)
include_directories(./src/LLD/include/)
include_directories(./src/LLD/templates/)
include_directories(./src/LLP/)
include_directories(./src/LLP/include/)
include_directories(./src/LLP/templates/)
include_directories(./src/TAO/Consensus/)
include_directories(./src/TAO/Consensus/include/)
include_directories(./src/TAO/Legacy/)
include_directories(./src/TAO/Legacy/include/)
include_directories(./src/Util/)
include_directories(./src/Util/include/)
include_directories(./src/Util/templates/)


set(LLC ./src/LLC)
set(LLD ./src/LLD)
set(LLP ./src/LLP)
set(TAO ./src/TAO)
set(Util ./src/Util)

file(GLOB LLCSources
        ${LLC}/*
        ${LLC}/hash/*
        ${LLC}/hash/SK/*
        ${LLC}/include/*
        ${LLC}/types/*)

file(GLOB LLDSources
        ${LLD}/*
        ${LLD}/include/*
        ${LLD}/templates/*)

file(GLOB LLPSources
        ${LLP}/hosts.cpp
        ${LLP}/network.cpp
        ${LLP}/incude/*
        ${LLP}/templates/*)

file(GLOB TAOSources
        ${TAO}/*
        ${TAO}/Consensus/*
        ${TAO}/Consensus/include/*
        ${TAO}/Core/*
        ${TAO}/Core/include/*
        ${TAO}/Legacy/*
        ${TAO}/Legacy/include/*)

file(GLOB UtilSources
        ${Util}/*
        ${Util}/include/*
        ${Util}/templates/*)

add_executable(ldd_benchmark ./src/main.cpp
        ${LLCSources}
        ${LLDSources}
        ${LLPSources}
        ${TAOSources}
        ${UtilSources})

target_link_libraries(ldd_benchmark
        LINK_PUBLIC ${Boost_LIBRARIES}
        LINK_PUBLIC ${OPENSSL_LIBRARIES}
        LINK_PUBLIC ${BERKELEY_DB_LIBRARIES}
        LINK_PUBLIC ${LevelDB_LIBRARY})