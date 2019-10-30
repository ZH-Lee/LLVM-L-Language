# LLVM tutorial in C++ 

This tutorial is a work in process and because my job in the future will be related with LLVM (Front-end or Back-end, whatever). And hope that this will help beginner to learn LLVM.
Finally, i named my language as "L" language. And i will put in more interesting things. 

Let's Start :)

## Table of Contents

*	[Chapter #1 Introduction](#chapter-1-introduction)
	* [Basic Procedure of Compiling a program](#Basic-Procedure-of-Compiling-a-program)
	* [The Project Structure](#the-project-structure)
	* [Environment](#Environment)
	* [TODO List](#TODO-List)

*	[Chapter #2 Lexer](#chater-2-Lexer)
	* 

## Chapter #1 Introduction

This tutorial showing how to implement your own language step by step. Assumes that you have already know C++, you just need some basic knowledge, go deeper is better :) 


### Basic Procedure of Compiling a program
At first, you need to know how to compile the compiled language(like C/C++, not Python). 
The procedure are showing below:


**source code** --->**Lexer** ---> **AST(Abstract Syntax Trees)** ---> **IR** --->**code generation** ---> **code optimization** --->...

The project does not consider code genration and procedures behind.

### The Project Structure
I refer the [LLVM Documentation](http://llvm.org/docs/tutorial/MyFirstLanguageFrontend/index.html). So my project structure is the same as the Documentation. And add more interesting things that you cannot find in that link. Hope that, yeah? Don't worry, we will talk about that in the future.

The project structure:

```
--L Language
	--src
		Lexer.cpp
		AST.cpp
		Parse.cpp
		main.cpp
	--examples
	grammar.txt
```

### Environment
```
Device: MacOS 10.1	
LLVM:	9.0.0
```

### TODO List

* Add For expression
* Add If/else expression
* Add more data type like int, bool, etc.
* Add pointers
* Add array 


