
# The location where you have installed ThorsLibraries.
# I have used brew install thors-mongo on an M1 mac this is the default location
# that brew will install packages.
THORSLIB_ROOT	= /opt/homebrew
BOOST_ROOT		= /opt/homebrew

#
# These are the flags that build the project.
# The linker uses CC by default to link objects.
CC			= $(CXX)
CPPFLAGS	= -I$(THORSLIB_ROOT)/include
CXXFLAGS	= -std=c++20
LDFLAGS		= -L$(THORSLIB_ROOT)/lib -L$(BOOST_ROOT)/lib
LDLIBS		= -lThorsLogging -lThorsSocket -levent -lboost_coroutine-mt -lboost_context-mt

NisseV6:	NisseV6.o ../V2/HTTPStuff.o ../V4/JobQueue.o ../V5/EventHandler.o


#
# These are targets that my NeoVim plugins use for syntax highlighting.
neovimflags:
	@echo $(CPPFLAGS) $(CXXFLAGS)


