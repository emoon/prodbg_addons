
local common = {
	Env = {
		M68KEMUPATH = "src/core",
		M68KMAKE = "$(OBJECTDIR)$(SEP)m68kmake$(PROGSUFFIX)",
	}
}

-----------------------------------------------------------------------------------------------------------------------

local mac_opts = {
	"-Wall",
	"-Wno-switch-enum",
	"-Werror",
	"-Wno-missing-variable-declarations",
	"-Wno-unknown-warning-option",
	"-Wno-c11-extensions",
	"-Wno-variadic-macros",
	"-Wno-c++98-compat-pedantic",
	"-Wno-old-style-cast",
	"-Wno-documentation",
	"-Wno-reserved-id-macro",
	"-Wno-missing-prototypes",
	"-Wno-deprecated-declarations",
	"-Wno-cast-qual",
	"-Wno-gnu-anonymous-struct",
	"-Wno-nested-anon-types",
	"-Wno-padded",
	"-Wno-c99-extensions",
	"-Wno-missing-field-initializers",
	"-Wno-weak-vtables",
	"-Wno-format-nonliteral",
	"-Wno-non-virtual-dtor",
	"-DOBJECT_DIR=\\\"$(OBJECTDIR)\\\"",
	{ "-O0", "-g"; Config = "*-*-debug" },
	{ "-O3", "-g"; Config = "*-*-release" },
}

local macosx = {
	Inherit = common,
    Env = {
        CCOPTS =  {
			mac_opts,
		},
    },
}

-----------------------------------------------------------------------------------------------------------------------

local gcc_opts = {
	"-I.",
	"-Wno-array-bounds", "-Wno-attributes", "-Wno-unused-value",
	"-DOBJECT_DIR=\\\"$(OBJECTDIR)\\\"",
	"-Wall", "-DPRODBG_UNIX",
	"-fPIC",
	{ "-O0", "-g"; Config = "*-*-debug" },
	{ "-O3", "-g"; Config = "*-*-release" },
}

local gcc_env = {
	Inherit = common,
    Env = {
        CCOPTS = {
			gcc_opts,
		},

        CXXOPTS = {
			gcc_opts,
			"-std=c++11",
		},
    },

	ReplaceEnv = {
		PROGCOM = "$(LD) $(PROGOPTS) $(LIBPATH:p-L) -o $(@) -Wl,--start-group $(LIBS:p-l) $(<) -Wl,--end-group"
	},
}

-----------------------------------------------------------------------------------------------------------------------

local win64_opts = {
	"/DPRODBG_WIN",
	"/EHsc", "/FS", "/MT", "/W3", "/I.", "/WX", "/DUNICODE", "/D_UNICODE", "/DWIN32", "/D_CRT_SECURE_NO_WARNINGS", "/wd4200", "/wd4152", "/wd4996", "/wd4389", "/wd4201", "/wd4152", "/wd4996", "/wd4389",
	"\"/DOBJECT_DIR=$(OBJECTDIR:#)\"",
	{ "/Od"; Config = "*-*-debug" },
	{ "/O2"; Config = "*-*-release" },
}

local win64 = {
	Inherit = common,
    Env = {
        GENERATE_PDB = "1",
        CCOPTS = {
			win64_opts,
        },

        CXXOPTS = {
			win64_opts,
        },

		OBJCCOM = "meh",
    },
}

-----------------------------------------------------------------------------------------------------------------------

Build {
    Passes = {
		BuildTools = { Name = "Build Tools", BuildOrder = 1 },
        CodeGeneration = { Name = "CodeGeneration", BuildOrder = 2 },
    },

    Units = {
    	"units.lua",
	},

    Configs = {
        Config { Name = "macosx-clang", DefaultOnHost = "macosx", Inherit = macosx, Tools = { "clang-osx" } },
        Config { Name = "win64-msvc", DefaultOnHost = { "windows" }, Inherit = win64, Tools = { "msvc",  } },
        Config { Name = "linux-gcc", DefaultOnHost = { "linux" }, Inherit = gcc_env, Tools = { "gcc" } },
    },

    IdeGenerationHints = {
        Msvc = {
            -- Remap config names to MSVC platform names (affects things like header scanning & debugging)
            PlatformMappings = {
                ['win64-msvc'] = 'x64',
            },
            -- Remap variant names to MSVC friendly names
            VariantMappings = {
                ['release']    = 'Release',
                ['debug']      = 'Debug',
            },
        },
    },

	Variants = { "debug", "release" },
}
