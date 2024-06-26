/*
* Copyright 2019 Tresys Technology, LLC
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*    http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#ifndef UTIL_H
#define UTIL_H

#include <stdbool.h>

// ignore conversions discarding const specifier, e.g.
//     const char []     ->  char *
//     const char *[2]'  ->  char *const *
// necessary for calling fts_open() and initializing cfg_opt_t
#ifdef __clang__
#define IGNORE_CONST_DISCARD_BEGIN _Pragma("clang diagnostic push") \
                                   _Pragma("clang diagnostic ignored \"-Wincompatible-pointer-types-discards-qualifiers\"")
#define IGNORE_CONST_DISCARD_END   _Pragma("clang diagnostic pop")
#elif defined(__GNUC__)
#define IGNORE_CONST_DISCARD_BEGIN _Pragma("GCC diagnostic push") \
                                   _Pragma("GCC diagnostic ignored \"-Wdiscarded-qualifiers\"") \
                                   _Pragma("GCC diagnostic ignored \"-Wincompatible-pointer-types\"")
#define IGNORE_CONST_DISCARD_END   _Pragma("GCC diagnostic pop")
#else
#define IGNORE_CONST_DISCARD_BEGIN
#define IGNORE_CONST_DISCARD_END
#endif

__attribute__((format(printf,1,2)))
void print_if_verbose(const char *format, ...);

/****************************************************
* Checks whether a string ends with a suffix.
* str - The string to check.
* str_len - The length of the string to check.
* suffix - The suffix to check against.
* suffix_len - The length of the suffix to check against.
* Returns true if the string ends with the given suffi, else false.
****************************************************/
bool ends_with(const char *str, size_t str_len, const char *suffix, size_t suffix_len);

/****************************************************
* Remove whitespaces of the end of a string.
* str - The string to trim.
* Return the passed and now trimmed string.
****************************************************/
char* trim_right(char *str);

/****************************************************
*
*   Conventions in refpolicy style policies
*
****************************************************/

static const char *const RefPol_module_abbreviations[][2] = {
	{ "fs"      , "filesystem"   },
	{ "corecmd" , "corecommands" },
	{ "seutil"  , "selinuxutil"  },
	{ "libs"    , "libraries"    },
	{ "dev"     , "devices"      },
	{ "term"    , "terminal"     },
	{ "corenet" , "corenetwork"  },
	{ "auth"    , "authlogin"    },
	{ "userdom" , "userdomain"   },
	{ "sysnet"  , "sysnetwork"   },
};

static const char *const RefPol_macros_with_module_prefix[] = {
	"domain_auto_transition_pattern",
	"domain_transition_pattern",
};

/**********************************
* Since the refpolicy style guide doesn't define what a "transform
* interface" is, we use some heuristics.  This checks if the interface
* name ends with one of several strings used in interfaces that appear
* to be what the style guide intends
**********************************/
static const char *const RefPol_interface_transforming_suffixes[] = {
	"_type",
	"_file",
	"_domain",
	"_node",
	// Next three are found in mta module
	"_agent",
	"_delivery",
	"_sender",
	"_boolean",
	"_content",
	"_constrained",
	"_executable",
	"_exemption",
	"_object",
	"_mountpoint",
};
static inline bool is_transform_interface(const char *if_name)
{
	const char *suffix = strrchr(if_name, '_');
	if (!suffix) {
		return false;
	}

	for (size_t i = 0; i < (sizeof RefPol_interface_transforming_suffixes / sizeof *RefPol_interface_transforming_suffixes); i++) {
		if (0 == strcmp(suffix, RefPol_interface_transforming_suffixes[i])) {
			return true;
		}
	}
	return false;
}

#endif
