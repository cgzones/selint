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

#ifndef SELINT_ERROR_H
#define SELINT_ERROR_H

enum selint_error {
	SELINT_SUCCESS,
	SELINT_BAD_ARG,
	SELINT_OUT_OF_MEM,
	SELINT_NO_MOD_NAME,
	SELINT_NOT_IN_BLOCK,
	SELINT_IF_CALL_LOOP,
	SELINT_PARSE_ERROR,
	SELINT_M4_SUB_FAILURE,
	SELINT_CONFIG_PARSE_ERROR,
	SELINT_IO_ERROR
};

#if defined(FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION) || defined(__AFL_COMPILER)
#define SELINT_FUZZING
#include <stdio.h>
#define SELINT_FUZZING_ABORT do { \
				fprintf(stderr, "abort in file %s, function %s, line %d\n", __FILE__, __func__, __LINE__); \
				abort(); \
			    } while(0)
#else
#define SELINT_FUZZING_ABORT
#endif

#endif
