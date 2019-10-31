# LLVM tutorial in C++ 

This tutorial is a work in process. Beginer will learn to how to build your own
language and i will go deeper into llvm after all basic function have been built.    
By the way, i named my language as "L" language. 
And i will put more interesting things in L language. 

Let's Start :)

## Table of Contents

*	[Chapter #1 Introduction](#chapter-1-introduction)  
	* [Basic Procedure of Compiling a program](#Basic-Procedure-of-Compiling-a-program)  
	* [The Project Structure](#the-project-structure)   
	* [Environment](#Environment)   
	* [TODO List](#TODO-List)   

*	[Chapter #2 Lexer](#chapter-2-Lexer) 


## Chapter #1 Introduction

This tutorial showing how to implement your own language step by step. Assumes that you have already know C++, you just need some basic knowledge, go deeper is better :) 


### Basic Procedure of Compiling a program
At first, you need to know how to compile the compiled language(like C/C++, not Python). 
The procedure are showing below:


**source code** --->**Lexer** ---> **AST(Abstract Syntax Trees)** ---> **IR** --->**code generation** ---> **code optimization** --->...

The project does not consider code genration and procedures behind.

### The Project Structure
I refer the [LLVM Documentation](http://llvm.org/docs/tutorial/MyFirstLanguageFrontend/index.html). So my project structure is the same as the Documentation. And add more interesting things that you cannot find in that link. Hope that, yeah? Don't worry, we will talk about that in the future.

**There are something different compared to the Documentation:**

* more data type
* more straightforward, it combines C++ and Python together. 

The project structure:

```
|-- Desktop
    |-- CMakeLists.txt
    |-- README.md
    |-- directoryList.md
    |-- grammar.txt
        |-- examples
    |-- src
        |-- AST.cpp
        |-- Lexer.cpp
        |-- Parser.cpp
        |-- main.cpp
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


## Chapter #2 Lexer