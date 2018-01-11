cmake_minimum_required(VERSION 3.9)
project(Rigidsolver)

set(CMAKE_CXX_STANDARD 11)

add_executable(Rigidsolver
        resources/models/ChessPawn.obj
        resources/models/teapot.obj
        resources/beauty.frag
        resources/beauty.vert
        README.txt
        RigidSolver.cpp
        RigidSolver.h
        stdafx.h
        targetver.h)
