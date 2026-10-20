-- license:BSD-3-Clause
-- copyright-holders:MAMEdev Team

---------------------------------------------------------------------------
--
--   devices.lua
--
--   Rules for building device cores
--
---------------------------------------------------------------------------

function devicesProject(_target, _subtarget)

	disasm_files = { }
	disasm_dependency = { }
	disasm_custombuildtask = { }

	project ("optional")
	uuid (os.uuid("optional-" .. _target .."_" .. _subtarget))
	kind (LIBTYPE)
	targetsubdir(_target .."_" .. _subtarget)

	if (_OPTIONS["targetos"] ~= "asmjs") then
		options {
			"ArchiveSplit",
		}
	end

	addprojectflags()
	precompiledheaders()

	includedirs {
		MAME_DIR .. "src/osd",
		MAME_DIR .. "src/emu",
		MAME_DIR .. "src/devices",
		MAME_DIR .. "src/lib",
		MAME_DIR .. "src/lib/util",
		MAME_DIR .. "3rdparty",
		MAME_DIR .. "3rdparty/asmjit", -- required by asmjit internal use of #include <...> syntax
		GEN_DIR  .. "emu",
		GEN_DIR  .. "emu/layout",
		ext_includedir("asio"),
		ext_includedir("expat"),
		ext_includedir("flac"),
	}

	-- pjsua2 support (3rdparty or external)
	-- Set MAME_HAVE_PJSUA2 or MAME_NO_PJSUA2 option that can be used from source to check pjsua2 availability.
	if _OPTIONS["NO_USE_PJSUA2"] == "0" and _OPTIONS["with-system-pjproject"] == "1" then
		local haspjsua2 = os.execute(pjsua2_pkgconfigcmd() .. " --exists libpjproject")
		if not haspjsua2 then
			_OPTIONS["NO_USE_PJSUA2"] = "1"
		end
	end

	if _OPTIONS["NO_USE_PJSUA2"] == "1" then
		defines {
			"MAME_NO_PJSUA2",
		}
	else
		defines {
			"MAME_HAVE_PJSUA2",
		}
		if _OPTIONS["with-system-pjproject"] == "1" then
			buildoptions {
				backtick(pjsua2_pkgconfigcmd() .. " --cflags libpjproject"),
			}
		else

			configuration { "gmake or ninja" }
				for _, dir in ipairs(pjproject_includedirs()) do
					buildoptions {
						"-isystem " .. dir,
					}
				end
			configuration { "vs*" }
				includedirs {
					pjproject_includedirs(),
				}
			configuration { }
		end
	end

	dofile(path.join("src", "cpu.lua"))

	dofile(path.join("src", "sound.lua"))

	dofile(path.join("src", "video.lua"))

	dofile(path.join("src", "machine.lua"))

	dofile(path.join("src", "bus.lua"))

	pchsource(MAME_DIR .. "src/devices/machine/timer.cpp")

if #disasm_files > 0 then
	project ("dasm")
	uuid ("f2d28b0a-6da5-4f78-b629-d834aa00429d")
	kind (LIBTYPE)
	targetsubdir(_target .."_" .. _subtarget)
	addprojectflags()
	precompiledheaders()

	includedirs {
		MAME_DIR .. "src/osd",
		MAME_DIR .. "src/emu",
		MAME_DIR .. "src/devices",
		MAME_DIR .. "src/lib",
		MAME_DIR .. "src/lib/util",
		MAME_DIR .. "3rdparty",
		GEN_DIR  .. "emu",
		ext_includedir("asio"),
		ext_includedir("expat"),
	}

	files {
		disasm_files
	}

	for key,value in pairs(disasm_files) do
		if string.endswith(value, ".cpp") then
			--print("calling pchsource with " .. value)
			pchsource(value)
			break
		end
	end

	if #disasm_dependency > 0 then
		dependency(disasm_dependency)
	end

	if #disasm_custombuildtask > 0 then
		custombuildtask(disasm_custombuildtask)
	end
end

end
