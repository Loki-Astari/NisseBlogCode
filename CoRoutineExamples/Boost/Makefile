# The location where you have installed BOOST
# I have used brew install thors-mongo on an M1 mac this is the default location
# that brew will install packages.
BOOST_ROOT		= /opt/homebrew

#
# These are the flags that build the project.
CXXFLAGS	= -std=c++20
LDFLAGS		= -L$(BOOST_ROOT)/lib
LDLIBS		= -lboost_coroutine-mt -lboost_context-mt

PrimeGen:


#
# These are targets that my NeoVim plugins use for syntax highlighting.
neovimflags:
	@echo $(CXXFLAGS)


