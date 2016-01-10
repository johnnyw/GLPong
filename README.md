# GLPong
Simple Pong implementation in C++ using OpenGL 3.x/GLEW/GLFW.

# Why
Using SDL renderer, SpriteKit, or XNA would have been a lot easier, but I wanted to see what writing a minimalistic game using "modern" OpenGL would look like.

# Dependencies
Requires GLEW for loading extensions (I'm really not using any, but on Windows, anything past GL 1.1 needs to be loaded as extensions), and GLFW for windowing/input (really didn't feel like writing platform-specific code for that). Should build and run on Windows, Mac, and Linux with proper libraries. In fact, on Mac and Linux you could just get rid of GLEW completely if you want.
