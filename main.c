#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/**
 * \def MAX_ENTRIES
 * Maximum possible number of extensions (some sensible bound; as of mid-2026
 * Khronos has 1110, with approximately 95 others, plus ANGLE EGL). Increase if
 * \c extension errors with \e excess \e elements when compiling.
 */
#define MAX_ENTRIES 1300

/**
 * Container type for calculated hashes.
 */
typedef uint32_t HashLookup[MAX_ENTRIES];

/**
 * All the Khronos extensions (with a \c NULL as the last used entry).
 */
static const char* extension[MAX_ENTRIES] = {
	#include "gl.inl"   // GL extensions
	#include "arb.inl"  // ARB extensions
	#include "es.inl"   // GL ES extensions
	#include "misc.inl" // Anything else
	NULL
};

/**
 * Hash calculated from the full extension string (including any prefix), with
 * indices matching <tt>extension</tt>.
 */
static HashLookup lookup = {0};

/**
 * Hash calculated from the extension with the \c GL_ prefix removed (any other
 * prefix, e.g.: <tt>GLX_</tt>, is not removed).
 *
 * \sa ::lookup
 */
static HashLookup unpref = {0};

/**
 * Searches both the extension and the un-prefixed extension hashes.
 *
 * \param[in] hash 32-bit hash to search for
 * \return either the matching index or \c MAX_ENTRIES if no match is found
 */
static unsigned find(uint32_t const hash) {
	assert(hash);
	for (unsigned n = 0; n < MAX_ENTRIES; n++) {
		if (lookup[n] == hash || unpref[n] == hash) {
			return n;
		}
	}
	return MAX_ENTRIES;
}

/**
 * Hash function adapted from Knuth's TAOCP.
 *
 * \param[in] str start of the data to hash
 * \param[in] len number of bytes
 * \return a 32-bit hash
 */
static uint32_t hash32(const char* str, size_t const len) {
	uint32_t hash = (uint32_t) len;
	for (uint32_t n = hash; n > 0; n--) {
		hash = ((hash << 5) ^ (hash >> 27)) ^ *str++;
	}
	return hash;
}

/**
 * \def PREFIX4_TO_INT
 * Takes the first four characters in a string and uses each byte to form a
 * little-endian 32-bit integer. The intended use is for GL prefixes, which
 * range from three- to four-characters (\e plus the \c NULL terminator).
 * Examples being:
 * \code
 *	"GL_"  0x005F4C47
 *	"GL_X" 0x5F584C47
 * \endcode
 * \note Optimised release builds fold this to a single constant (with debug
 * builds verifying the string length), then single compare instruction.
 * \note This is preferred over a multi-character literal (which though has wide
 * support, is big-endian, so needs extra shifts when building the comparison
 * strings, and requires \c -Wno-error=multichar in modern compilers), or hacks
 * like <tt>*((uint32_t*) "GL_\0")</tt> (machine-endian, and potentially
 * unaligned).
 *
 * \param str 3-4 character string prefix
 * \return a 32-bit unsigned integer composed from the characters
 */
#define PREFIX4_TO_INT(str) (assert(strlen(str) >= 3),\
	((unsigned) str[0]      ) |\
	((unsigned) str[1] <<  8) |\
	((unsigned) str[2] << 16) |\
	((unsigned) str[3] << 24))

/**
 * Removes the prefix from the known prefixes (<tt>GL_</tt>, the most common,
 * but also <tt>GLX_</tt>, <tt>EGL_</tt>, <tt>WGL_</tt> and <tt>GLU_</tt>).
 *
 * \note We do find duplicates once the prefix is removed, \c GL_ARB_multisample
 * has also a matching \c GLX_ARB_multisample and \c WGL_ARB_multisample for the
 * same ARB extension (<tt>ARB_multisample</tt>, so the duplicates are correct).
 *
 * \param[in] ext start of the extension name
 * \param[in] len number of characters
 * \return prefix length (should be \c 3 or \c 4 from the known prefixes)
 */
const char* removePrefix(const char* const ext, size_t const len) {
	if (len > 3) {
		unsigned calc = 0;
		for (unsigned n = 0; n < 3; n++) {
			calc |= ext[n] << (n << 3);
		}
		if (calc == PREFIX4_TO_INT("GL_")) { // 1080
			return ext + 3;
		}
		if (len > 4) {
			calc |= ext[3] << 24;
			if (calc == PREFIX4_TO_INT("GLX_") || // 68
				calc == PREFIX4_TO_INT("EGL_") || // 68 (all in misc)
				calc == PREFIX4_TO_INT("WGL_") || // 55
				calc == PREFIX4_TO_INT("GLU_"))   //  4
			{
				return ext + 4;
			}
		}
	}
	/*
	 * We know all the prefixes and this should never happen (unless new
	 * extensions get introduced and this list not updated, or it's running in
	 * production on WebGL with unprefixed extensions).
	 */
	return ext;
}

/**
 * Standalone test of the embedded Khronos extension strings.
 *
 * \note In testing this needs at least 19 bits with the 1200 or so extensions
 * to not have clashes between prefixed and unprefixed versions (approx. 2200).
 */
int main() {
	unsigned unprefed = 0;
	for (unsigned n = 0; n < MAX_ENTRIES; n++) {
		const char* ext = extension[n];
		if (ext != NULL) {
			size_t len = strlen(ext);
			if (len) {
				// Sanity test that the extension names were copied correctly
				if (strcspn(ext, " \t") != len) {
					printf("Leading or trailing space for '%s'\n", ext);
					return EXIT_FAILURE;
				}
				// Hash the full extension name
				uint32_t hash = hash32(ext, len);
				unsigned found = find(hash);
				lookup[n] = hash;
				// Any duplicates?
				if (found < MAX_ENTRIES) {
					const char* clash = extension[found];
					if (strcmp(ext, clash) == 0) {
					#ifndef NDEBUG
						// Ignore real duplicate strings (probably ES)
						printf("Ignoring duplicate '%s' "
							"(%d and %d)\n", ext, n, found);
					#endif
					} else {
						printf("Collision for '%s' at %d (with "
							"'%s' at %d)\n", ext, n, clash, found);
						return EXIT_FAILURE;
					}
				} else {
					// Try again with the prefix removed
					const char* extName = removePrefix(ext, len);
					if (ext != extName) {
						hash = hash32(extName, len - (extName - ext));
						found = find(hash);
						unpref[n] = hash;
						unprefed++;
						// Any duplicates?
						if (found < MAX_ENTRIES) {
							const char* clash = extension[found];
							if (strcmp(extName, removePrefix(clash, strlen(clash))) == 0) {
							#ifndef NDEBUG
								// Ignore duplicate name-only extensions
								printf("Ignoring prefixed duplicate '%s' "
									"(%d and %d)\n", extName, n, found);
							#endif
							} else {
								printf("Prefixed collision for '%s' at %d (with "
									"'%s' at %d)\n", extName, n, clash, found);
								return EXIT_FAILURE;
							}
						}
					} else {
						printf("Unknown prefix: '%s'\n", ext);
						return EXIT_FAILURE;
					}
				}
			} else {
				printf("Empty string at index %d\n", n);
				return EXIT_FAILURE;
			}
		} else {
			printf("No collisions found!\n%d extensions tested, "
				"%d unprefixed, a total of %d strings.\n",
					n + 1, unprefed, n + 1 + unprefed);
			break;
		}
	}
	return EXIT_SUCCESS;
}
