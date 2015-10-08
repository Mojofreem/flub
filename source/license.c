#include <stdio.h>
#include <string.h>
#include <flub/license.h>
#include <flub/logger.h>


#define MAX_LICENSE_LINE_SIZE 256


struct licenseText_s {
	const char *license;
	const char *terms;
} g_licenseTerms_tbl[] = {
		{"zlib", "This software is provided 'as-is', without any express or implied\n"
 				 "warranty.  In no event will the authors be held liable for any damages\n"
				 "arising from the use of this software.\n\n"
				 "Permission is granted to anyone to use this software for any purpose,\n"
				 "including commercial applications, and to alter it and redistribute it\n"
				 "freely, subject to the following restrictions:\n\n"
				 "1. The origin of this software must not be misrepresented; you must not\n"
				 "   claim that you wrote the original software. If you use this software\n"
				 "   in a product, an acknowledgment in the product documentation would be\n"
				 "   appreciated but is not required.\n"
				 "2. Altered source versions must be plainly marked as such, and must not be\n"
				 "   misrepresented as being the original software.\n"
				 "3. This notice may not be removed or altered from any source distribution.\n"},
		{"public domain", "This software is in the public domain. Where that dedication is not\n"
   						  "recognized, you are granted a perpetual, irrevocable license to copy\n"
   						  "and modify this file however you want.\n"},
#ifdef WIN32
		{"glew", "Redistribution and use in source and binary forms, with or without\n"
		         "modification, are permitted provided that the following conditions are met:\n\n"
				 "* Redistributions of source code must retain the above copyright notice,\n"
				 "  this list of conditions and the following disclaimer.\n"
				 "* Redistributions in binary form must reproduce the above copyright notice,\n"
				 "  this list of conditions and the following disclaimer in the documentation\n"
				 "  and/or other materials provided with the distribution.\n"
				 "* The name of the author may be used to endorse or promote products\n"
				 "  derived from this software without specific prior written permission.\n\n"
				 "THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS \"AS IS\"\n" 
				 "AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE\n"
				 "IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE\n"
 				 "ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE\n"
				 "LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR\n"
				 "CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF\n"
				 "SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS\n"
				 "INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN\n"
				 "CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)\n"
				 "ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF\n"
				 "THE POSSIBILITY OF SUCH DAMAGE.\n"},
#endif // WIN32
#ifdef USE_EXPAT
		{"MIT", "Permission is hereby granted, free of charge, to any person obtaining\n"
				"a copy of this software and associated documentation files (the\n"
				"\"Software\"), to deal in the Software without restriction, including\n"
				"without limitation the rights to use, copy, modify, merge, publish,\n"
				"distribute, sublicense, and/or sell copies of the Software, and to\n"
				"permit persons to whom the Software is furnished to do so, subject to\n"
				"the following conditions:\n\n"
				"The above copyright notice and this permission notice shall be included\n"
				"in all copies or substantial portions of the Software.\n\n"
				"THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,\n"
				"EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF\n"
				"MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.\n"
				"IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY\n"
				"CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,\n"
				"TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE\n"
				"SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.\n"},
#endif // USE_EXPAT
		{NULL, NULL},
	};


struct licenseDetail_s {
	const char *component;
	const char *copyright;
	const char *license;
} g_licenseDetail_tbl[] =
	{
        {"flub", "flub (c)2015 by Liam Chasteen", "zlib"},
        {"SDL2", "SDL2 (c)1997-2014 by Sam Lantinga", "zlib"},
        //{"SDL_mixer", "SDL_mixer (c)1997-2013 San Lantinga", "zlib"},
        //{"SDL_net", "SDL_net (c)1997-2013 San Lantinga", "zlib"},
        {"stb_image", "stb_image (c)Sean Barret", "public domain"},
        {"stb_font_consolas_12", "stb_font_consolas (c)Sean Barret", "public domain"},
#ifdef WIN32
        {"glew", "glew (c)2002-2015 by Lev Povalahev, Marcelo E. Magallon, Milan Ikits, Nigel Stewart", "glew"},
#endif // WIN32
        {"expat", "expat (c)1990-2000 Thai Open Source Software Center Ltd, (c)2001-2006 Expat maintainers", "MIT"},
		{NULL, NULL, NULL},
	};


void licenseEnumComponents(licenseCB_t callback, void *context) {
	int k;

	for(k = 0; g_licenseDetail_tbl[k].component != NULL; k++) {
		if(!callback(g_licenseDetail_tbl[k].component, context)) {
			break;
		}
	}	
}

void licenseEnumLicenses(licenseCB_t callback, void *context) {
	int k;

	for(k = 0; g_licenseTerms_tbl[k].license != NULL; k++) {
		if(!callback(g_licenseTerms_tbl[k].license, context)) {
			break;
		}
	}	
}

void licenseEnumCopyrights(licenseCB_t callback, void *context) {
	char buf[MAX_LICENSE_LINE_SIZE];
	int k;

	for(k = 0; g_licenseDetail_tbl[k].component != NULL; k++) {
		snprintf(buf, sizeof(buf), "%s, %s license.",
				 g_licenseDetail_tbl[k].copyright,
				 g_licenseDetail_tbl[k].license);
		buf[sizeof(buf) - 1] = '\0';
		if(!callback(buf, context)) {
			break;
		}
	}	
}

static int licenseGetTerms(const char *license) {
	int k;

	for(k = 0; g_licenseTerms_tbl[k].license != NULL; k++) {
		if(!strcmp(license, g_licenseTerms_tbl[k].license)) {
			return k;
		}
	}
	return  -1;
}

static int generateLicense(licenseCB_t callback, void *context,
						   const char *component,
						   const char *copyright, const char *license) {
	static const char *dashLine = "license terms ================================================================";
	static const char *singleLine = "------------------------------------------------------------------------------";
	char buf[MAX_LICENSE_LINE_SIZE];
	int k, width, idx;
	const char *ptr, *end;

	if(!callback(singleLine, context)) {
		return 0;
	}

	snprintf(buf, sizeof(buf), "%s, %s license", copyright, license);
	buf[sizeof(buf) - 1] = '\0';
	if(!callback(buf, context)) {
		return 0;
	}

	width = strlen(dashLine) - (4 + strlen(license));
	snprintf(buf, sizeof(buf), "== %s %*.*s", license, width, width, dashLine);
	buf[sizeof(buf) - 1] = '\0';
	if(!callback(buf, context)) {
		return 0;
	}

	idx = licenseGetTerms(license);
	if(idx < 0) {
		errorf("Missing license terms for component \"%s\", license \"%s\".",
			   component, g_licenseDetail_tbl[k].license);
		snprintf(buf, sizeof(buf), "* Missing license terms for license \"%s\".", license);
		buf[sizeof(buf) - 1] = '\0';
		if(!callback(buf, context)) {
			return 0;
		}
	} else {
		ptr = g_licenseTerms_tbl[idx].terms;
		do {
			if(*ptr == '\0') {
				break;
			}
			if((end = strchr(ptr, '\n')) != NULL) {
				width = (end - ptr) + 1;
				if(width > sizeof(buf)) {
					warningf("License terms line (%d) exceeds anticipated line length (%d).",
							 width, MAX_LICENSE_LINE_SIZE);
					width = sizeof(buf);
				}
				memcpy(buf, ptr, width);
				buf[sizeof(buf) - 1] = '\0';
				if(!callback(buf, context)) {
					return 0;
				}
				ptr = end + 1;
			} else {
				if(!callback(ptr, context)) {
					return 0;
				}
				break;
			}
		} while(1);
	}

	if(!callback(singleLine, context)) {
		return 0;
	}
	
	return 1;
}

void licenseGet(licenseCB_t callback, void *context, const char *component) {
	int k;

	for(k = 0; g_licenseDetail_tbl[k].component != NULL; k++) {
		if(!strcasecmp(g_licenseDetail_tbl[k].component, component)) {
			if(!generateLicense(callback, context, component,
								g_licenseDetail_tbl[k].copyright,
								g_licenseDetail_tbl[k].license)) {
				return;
			}
		}
	}
}

void licenseGetAll(licenseCB_t callback, void *context) {
	int k;

	for(k = 0; g_licenseDetail_tbl[k].component != NULL; k++) {
		if(!generateLicense(callback, context, g_licenseDetail_tbl[k].component,
							g_licenseDetail_tbl[k].copyright,
							g_licenseDetail_tbl[k].license)) {
			return;
		}
	}
}
