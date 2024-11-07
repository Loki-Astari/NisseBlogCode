
# The location where you have installed ThorsLibraries.
# I have used brew install thors-mongo on an M1 mac this is the default location
# that brew will install packages.
THORSLIB_ROOT	= /opt/homebrew

#
# These are the flags that build the project.
CPPFLAGS	= -I$(THORSLIB_ROOT)/include
CXXFLAGS	= -std=c++20
LDFLAGS		= -L$(THORSLIB_ROOT)/lib
LDLIBS		= -lThorsLogging -lThorsSocket

NisseV2:


#
# These are targets that my NeoVim plugins use for syntax highlighting.
neovimflags:
	@echo $(CPPFLAGS) $(CXXFLAGS)


