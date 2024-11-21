
# The location where you have installed ThorsLibraries.
# I have used brew install thors-mongo on an M1 mac this is the default location
# that brew will install packages.
THORSLIB_ROOT	= /opt/homebrew

#
# These are the flags that build the project.
# The linker uses CC by default to link objects.
CC			= $(CXX)
CPPFLAGS	= -I$(THORSLIB_ROOT)/include
CXXFLAGS	= -std=c++20
LDFLAGS		= -L$(THORSLIB_ROOT)/lib
LDLIBS		= -lThorsLogging -lThorsSocket -levent

NisseV5:	NisseV5.o ../V1/HTTPStuff.o ../V2/ServerInit.o ../V4/JobQueue.o EventHandler.o


#
# These are targets that my NeoVim plugins use for syntax highlighting.
neovimflags:
	@echo $(CPPFLAGS) $(CXXFLAGS)


