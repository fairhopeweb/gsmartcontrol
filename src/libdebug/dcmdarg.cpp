/******************************************************************************
License: Zlib
Copyright:
	(C) 2008 - 2021 Alexander Shaduri <ashaduri@gmail.com>
******************************************************************************/
/// \file
/// \author Alexander Shaduri
/// \ingroup libdebug
/// \weakgroup libdebug
/// @{

#if defined ENABLE_GLIB && ENABLE_GLIB

#include <string>
#include <vector>
#include <map>
#include <glib.h>
#include <sstream>
#include <ios>  // std::boolalpha
#include <algorithm>  // std::find

#include "hz/string_algo.h"  // string_split()

#include "dcmdarg.h"
#include "dflags.h"
#include "dstate.h"




namespace debug_internal {


	/// Internal class, holds values of command line args.
	struct DebugCmdArgs {
		/// Constructor
		DebugCmdArgs()
		{
		#ifdef _WIN32
			verbose = TRUE;
			debug_colorize = FALSE;
		#endif
		#ifdef DEBUG_BUILD
			verbosity_level = 5;  // all
		#endif
		}

		// use glib types here
		gboolean verbose = FALSE;  ///< Verbose output (enables higher verbosity level)
		gboolean quiet = FALSE;  ///< Less verbose output (enables lower verbosity level)
		gint verbosity_level = 3;  ///< Verbosity level override - warn, error, fatal
		std::vector<std::string> debug_levels;  ///< Comma-separated names of levels to enable
		gboolean debug_colorize = TRUE;  ///< Colorize the output or not

		debug_level::types levels_enabled;  ///< Final vector - not actually an argument, but filled after the parsing.
	};



	/// Get libdebug command-line arguments
	inline DebugCmdArgs* get_debug_get_args_holder()
	{
		static DebugCmdArgs args;
		return &args;
	}


} // ns debug_internal




extern "C" {

	/// Glib callback for parsing libdebug levels argument
	static gboolean debug_internal_parse_levels(const gchar* option_name,
			const gchar* value, gpointer data, GError** error);

	/// Glib callback for performing post-parse phase (parse hook)
	static gboolean debug_internal_post_parse_func(GOptionContext* context,
			GOptionGroup *group, gpointer data, GError** error);

}



// GLib callback
static gboolean debug_internal_parse_levels([[maybe_unused]] const gchar* option_name,
		const gchar* value, gpointer data, [[maybe_unused]] GError** error)
{
	if (!value)
		return false;
	auto* args = static_cast<debug_internal::DebugCmdArgs*>(data);
	std::string levels = value;
	hz::string_split(levels, ',', args->debug_levels, true);
	// will filter out invalid ones later
	return true;
}



// GLib callback
static gboolean debug_internal_post_parse_func([[maybe_unused]] GOptionContext* context,
		[[maybe_unused]] GOptionGroup *group, gpointer data, [[maybe_unused]] GError** error)
{
	auto* args = static_cast<debug_internal::DebugCmdArgs*>(data);

	if (!args->debug_levels.empty()) {  // no string levels on command line given
		args->levels_enabled = debug_level::none;  // reset
		args->levels_enabled |= (std::find(args->debug_levels.begin(), args->debug_levels.end(),
				"dump") != args->debug_levels.end() ? debug_level::dump : debug_level::none);
		args->levels_enabled |= (std::find(args->debug_levels.begin(), args->debug_levels.end(),
				"info") != args->debug_levels.end() ? debug_level::info : debug_level::none);
		args->levels_enabled |= (std::find(args->debug_levels.begin(), args->debug_levels.end(),
				"warn") != args->debug_levels.end() ? debug_level::warn : debug_level::none);
		args->levels_enabled |= (std::find(args->debug_levels.begin(), args->debug_levels.end(),
				"error") != args->debug_levels.end() ? debug_level::error : debug_level::none);
		args->levels_enabled |= (std::find(args->debug_levels.begin(), args->debug_levels.end(),
				"fatal") != args->debug_levels.end() ? debug_level::fatal : debug_level::none);

	} else if (args->quiet) {
		args->levels_enabled = debug_level::none;

	} else if (args->verbose) {
		args->levels_enabled = debug_level::all;

	} else {
		args->levels_enabled = debug_level::none;

		if (args->verbosity_level > 0) args->levels_enabled |= debug_level::fatal;
		if (args->verbosity_level > 1) args->levels_enabled |= debug_level::error;
		if (args->verbosity_level > 2) args->levels_enabled |= debug_level::warn;
		if (args->verbosity_level > 3) args->levels_enabled |= debug_level::info;
		if (args->verbosity_level > 4) args->levels_enabled |= debug_level::dump;
	}

	bool color_enabled = static_cast<bool>(args->debug_colorize);


	unsigned long levels_enabled_ulong = args->levels_enabled.to_ulong();
	debug_internal::DebugState::domain_map_t& dm = debug_internal::get_debug_state().get_domain_map();

	for (auto& iter : dm) {
		for (auto& iter2 : iter.second) {
			iter2.second->set_enabled(bool(levels_enabled_ulong & iter2.first));

			debug_format::type format = iter2.second->get_format();
			if (color_enabled) {
				format |= debug_format::color;
			} else {
				format &= ~(unsigned long)debug_format::color;
			}
			iter2.second->set_format(format);
		}
	}

	return true;
}





std::string debug_get_cmd_args_dump()
{
	debug_internal::DebugCmdArgs* args = debug_internal::debug_get_args_holder();
	std::ostringstream ss;

// 	ss << "\tverbose: " << std::boolalpha << static_cast<bool>(args->verbose) << "\n";
// 	ss << "\tquiet: " << std::boolalpha << static_cast<bool>(args->quiet) << "\n";
// 	ss << "\tverbosity_level: " << args->verbosity_level << "\n";
// 	ss << "\tdebug_levels: " << args->debug_levels << "\n";
	ss << "\tlevels_enabled: " << args->levels_enabled << "\n";
	ss << "\tdebug_colorize: " << std::boolalpha << static_cast<bool>(args->debug_colorize) << "\n";

	return ss.str();
}




// If adding the returned value to context (which you must do),
// no need to free the result.
GOptionGroup* debug_get_option_group()
{
	debug_internal::DebugCmdArgs* args = debug_internal::debug_get_args_holder();

	GOptionGroup* group = g_option_group_new("debug",
			"Libdebug Logging Options", "Show libdebug options",
			args, nullptr);

	static const GOptionEntry entries[] =
	{
		{ "verbose", 'v', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
				&(args->verbose), "Enable verbose logging; same as --verbosity-level 5", nullptr },
		{ "quiet", 'q', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
				&(args->quiet), "Disable logging; same as --verbosity-level 0", nullptr },
		{ "verbosity-level", 'b', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_INT,
				&(args->verbosity_level), "Set verbosity level [0-5]", nullptr },
		{ "debug-levels", '\0', 0, G_OPTION_ARG_CALLBACK,
				(gpointer)(&debug_internal_parse_levels),  // reinterpret_cast<> doesn't work here
				"Enable only these logging levels; the argument is a comma-separated list of (dump, info, warn, error, fatal)", nullptr },
		{ "debug-colorize", '\0', 0, G_OPTION_ARG_NONE,
				&(args->debug_colorize), "Enable colored output", nullptr },
		{ "debug-no-colorize", '\0', G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE,
				&(args->debug_colorize), "Disable colored output", nullptr },
		{ nullptr, '\0', 0, G_OPTION_ARG_NONE, nullptr, nullptr, nullptr }
	};

	g_option_group_add_entries(group, entries);

	g_option_group_set_parse_hooks(group, nullptr, &debug_internal_post_parse_func);

	return group;
}





#endif  // glib




/// @}
