local counterparts = {
	c = { "h" },
	cc = { "hh", "hpp", "h" },
	cpp = { "hpp", "h", "hxx" },
	cxx = { "hxx", "hpp", "h" },
	h = { "c", "cpp", "cc", "cxx" },
	hh = { "cc", "cpp", "cxx" },
	hpp = { "cpp", "cc", "cxx" },
	hxx = { "cxx", "cpp", "cc" },
}

local header_extensions = {
	h = true,
	hh = true,
	hpp = true,
	hxx = true,
}

local function switch_source_header()
	local current = vim.api.nvim_buf_get_name(0)
	local extension = vim.fn.fnamemodify(current, ":e")
	local target_extensions = counterparts[extension]

	if not target_extensions then
		vim.notify("Not a C/C++ source or header file", vim.log.levels.WARN)
		return
	end

	local root = vim.fs.root(current, { "CMakeLists.txt", ".git" }) or vim.uv.cwd()
	local basename = vim.fn.fnamemodify(current, ":t:r")
	local preferred_directory = header_extensions[extension] and "src" or "include"
	local search_paths = { root .. "/" .. preferred_directory, root }

	for _, target_extension in ipairs(target_extensions) do
		local target_name = basename .. "." .. target_extension
		for _, search_path in ipairs(search_paths) do
			if vim.uv.fs_stat(search_path) then
				local matches = vim.fs.find(target_name, {
					path = search_path,
					type = "file",
					limit = 1,
				})
				if matches[1] then
					vim.api.nvim_cmd({ cmd = "edit", args = { matches[1] } }, {})
					return
				end
			end
		end
	end

	vim.notify("No matching source/header file found", vim.log.levels.WARN)
end

return {
	{
		"neovim/nvim-lspconfig",
		opts = function(_, opts)
			opts.servers = opts.servers or {}
			opts.servers.clangd = opts.servers.clangd or {}
			opts.servers.clangd.keys = opts.servers.clangd.keys or {}

			table.insert(opts.servers.clangd.keys, {
				"s<Space>",
				switch_source_header,
				desc = "Switch Source/Header (C/C++)",
			})
		end,
	},
}
