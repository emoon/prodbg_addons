require "tundra.syntax.glob"

local function get_src(dir, recursive)
	return FGlob {
		Dir = dir,
		Extensions = { ".cpp", ".c", ".h", ".s", ".m" },
		Filters = {
			{ Pattern = "[/\\]test[/\\]"; Config = "test-*" }, -- Directories named "test" and their subdirectories will be excluded from builds

			{ Pattern = "%.s$"; Config = "amiga-*" },
			{ Pattern = "[/\\]amiga[/\\]"; Config = "amiga-*" },
			{ Pattern = "[/\\]win32[/\\]"; Config = "win32-*" },
			{ Pattern = "[/\\]macosx[/\\]"; Config = "mac*-*" },

		},
		Recursive = recursive and true or false,
	}
end

DefRule {
	Name = "m68kmake",
	Pass = "CodeGeneration",
	Command = "$(M68KMAKE) $(<) $(M68KEMUPATH)/m68k_in.c_",
	ImplicitInputs = { "$(M68KMAKE)", "$(M68KEMUPATH)/m68k_in.c_" },

	Blueprint = {
		TargetDir = { Required = true, Type = "string", Help = "Input filename", },
	},

	Setup = function (env, data)
		return {
			InputFiles = { data.TargetDir },
			OutputFiles = {
				"$(OBJECTDIR)/_generated/m68kopac.c",
				"$(OBJECTDIR)/_generated/m68kopdm.c",
				"$(OBJECTDIR)/_generated/m68kopnz.c",
				"$(OBJECTDIR)/_generated/m68kops.c",
				"$(OBJECTDIR)/_generated/m68kops.h",
			},
		}
	end,
}

Program {
	Name = "m68kmake",
	Pass = "BuildTools",
	Target = "$(M68KMAKE)",
	Sources = { "$(M68KEMUPATH)/m68kmake.c" },
}

SharedLibrary {
    Name = "musashi_addon",

    Env = {
        CPPPATH = {
			"$(OBJECTDIR)/_generated",
        	"../api",
        	"src/core",
        },
    	CXXOPTS = { { "-fPIC"; Config = "linux-gcc"; }, },
    },

	Sources = {
		get_src("src", true),

		m68kmake {
			TargetDir = "$(OBJECTDIR)/_generated",
		},
	},

	IdeGenerationHints = { Msvc = { SolutionFolder = "Addons" } },
}

-------------------------------------------------------------------------

Default "m68kmake"
Default "musashi_addon"

