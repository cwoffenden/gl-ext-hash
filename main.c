#include <assert.h>
#include <stdbool.h>
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
 * Container type for calculated hashes.
 */
typedef uint32_t HashLookup[MAX_ENTRIES];

/**
 * All the Khronos and other extensions (with a \c NULL as the last used entry).
 */
static const char* extension[MAX_ENTRIES] = {
	#include "data/gl.inl"    // GL extensions
	#include "data/arb.inl"   // ARB extensions
	#include "data/es.inl"    // GL ES extensions
	#include "data/webgl.inl" // WebGL extensions
	#include "data/misc.inl"  // Anything else
	NULL
};

/**
 * Hash calculated from the full extension string (including any prefix), with
 * indices matching <tt>extension</tt>.
 */
static HashLookup fullHash = {0};

/**
 * Precalculated \c fullHash hashes (from an assumed working run, used to verify
 * generation is the same across systems).
 */
static HashLookup precalcFull = {
	#include "data/precalc-full.inl"
};

/**
 * Hash calculated from the extension with the \c GL_ prefix removed (or any
 * other prefix, e.g.: <tt>GLX_</tt>) leaving just the extension name.
 *
 * \sa fullHash
 */
static HashLookup nameHash = {0};

/**
 * Precalculated \c nameHash hashes.
 *
 * \sa precalcFull
 */
static HashLookup precalcName = {
	#include "data/precalc-name.inl"
};

/**
 * Working container for the sorted hashes. Unique entries from \c fullHash and
 * \c nameHash are added here then sorted in ascending order (which means all
 * the unused zero entries will be at the start).
 *
 * \sa compareU32()
 */
static uint32_t sortedHash[MAX_ENTRIES * 2] = {0};

//********************************** Helpers **********************************/

/**
 * Linear search in both the extension and the un-prefixed name hashes.
 *
 * \param[in] hash 32-bit hash to search for
 * \return either the matching index or \c MAX_ENTRIES if no match is found
 */
static unsigned find(uint32_t const hash) {
	assert(hash);
	for (unsigned n = 0; n < MAX_ENTRIES; n++) {
		if (fullHash[n] == hash || nameHash[n] == hash) {
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
	assert(str);
	uint32_t hash = (uint32_t) len;
	for (uint32_t n = hash; n > 0; n--) {
		hash = ((hash << 5) ^ (hash >> 27)) ^ *str++;
	}
	return hash;
}

/**
 * Removes the extension prefix if it contains one of the known prefixes
 * (<tt>GL_</tt>, the most common, but also <tt>GLX_</tt>, <tt>EGL_</tt>,
 * <tt>WGL_</tt> and <tt>GLU_</tt>).
 *
 * \note We do find duplicates once the prefix is removed, \c GL_ARB_multisample
 * has also a matching \c GLX_ARB_multisample and \c WGL_ARB_multisample for the
 * same ARB extension (<tt>ARB_multisample</tt>, so the duplicates are correct).
 *
 * \param[in] ext start of the extension name
 * \param[in] len number of characters
 * \return \a ext advanced by any found prefix (or untouched if not found)
 */
static const char* unprefix(const char* const ext, size_t const len) {
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
 * Helper to decide between either a space or a newline following an entry when
 * dumping or printing hex data.
 *
 * \param[in] count running count of written entries
 * \param[in] wrap after how many entries the line wraps (dictating space or newline)
 * \return a single space or newline character
 */
static char spaceOrNewline(unsigned const count, unsigned const wrap) {
	return (count > 0 && ((count + 1) % wrap) == 0) ? '\n' : ' ';
}

/**
 * Comparison function for \c qsort and any other sort or search.
 *
 * \param[in] lhs pointer to a \c uint32_t
 * \param[in] rhs pointer to a \c uint32_t
 * \return an integer following the \c qsort comparator predicate rules for ascending order
 */
static int compareU32(const void* const lhs, const void* const rhs) {
	uint32_t lhsVal = *((uint32_t*) lhs);
	uint32_t rhsVal = *((uint32_t*) rhs);
	return (lhsVal < rhsVal) ? -1 : ((lhsVal > rhsVal) ? 1 : 0);
}

/**
 * Binary search for the \c hash32() of \a str in the sorted unique hashes.
 *
 * \note \c sortedHash is in ascending order, \c compareU32() is the comparator,
 * and the offset to the first entry is passed in (since this is for testing;
 * production should only have the non-zero entries).
 *
 * \param[in] str extension string
 * \param[in] off index into \c sortedHash for the first non-zero entry
 * \return \c true if a match was found
 */
static bool hasUnique(const char* const str, unsigned const off) {
	assert(str && off < MAX_ENTRIES * 2);
	uint32_t hash = hash32(str, strlen(str));
	const void* found = bsearch(&hash,
		sortedHash + off, MAX_ENTRIES * 2 - off,
			sizeof(uint32_t), compareU32);
	if (found) {
		return *((uint32_t*) found) == hash;
	}
	return false;
}

//*****************************************************************************/

/**
 * Performs the work of generating all the hashes (and filling the tables).
 *
 * \note This will halt and \c exit() on encountering an error.
 *
 * \param[in] chatty \c true to enable verbose debug output
 */
static void generateHashes(bool const chatty) {
	unsigned processed = 0; // Total entries processed from extension[]
	unsigned validFull = 0; // Valid, unique full extensions
	unsigned validName = 0; // Valid, unique extensions after removing any prefix
	unsigned duplicate = 0; // Valid duplicates (not clashes)
	for (unsigned n = 0; n < MAX_ENTRIES; n++) {
		const char* ext = extension[n];
		if (ext != NULL) {
			size_t len = strlen(ext);
			if (len) {
				// Sanity test that the extension names were copied correctly
				if (strcspn(ext, " \t") != len) {
					printf("Leading or trailing space for '%s'\n", ext);
					exit(EXIT_FAILURE);
				}
				// Hash the full extension name (and store the result)
				uint32_t hash = hash32(ext, len);
				unsigned match = find(hash);
				fullHash[n] = hash;
				// Any duplicates?
				bool told = false;
				if (match < MAX_ENTRIES) {
					const char* clash = extension[match];
					if (strcmp(ext, clash) == 0) {
						duplicate++;
						if (chatty) {
							// Ignore real duplicate strings (probably ES)
							printf("Ignoring duplicate entry '%s' "
								"(%d and %d)\n", ext, n, match);
							told = true;
						}
					} else {
						printf("Collision for '%s' at %d (with "
							"'%s' at %d)\n", ext, n, clash, match);
						exit(EXIT_FAILURE);
					}
				} else {
					// Unique entries only in the sorted list (sorted later)
					sortedHash[n] = hash;
					validFull++;
				}
				// Try again with the prefix removed
				const char* name = unprefix(ext, len);
				if (ext == name) {
					printf("Unknown prefix: '%s'\n", ext);
					exit(EXIT_FAILURE);
				}
				hash = hash32(name, len - (name - ext));
				match = find(hash);
				nameHash[n] = hash;
				// Any duplicates?
				if (match < MAX_ENTRIES) {
					const char* clash = extension[match];
					if (strcmp(name, unprefix(clash, strlen(clash))) == 0) {
						duplicate++;
						if (chatty && !told) {
							// Ignore duplicate name-only extensions
							printf("Ignoring duplicate name '%s' at %d "
								"(with '%s' at %d)\n", ext, n, clash, match);
						}
					} else {
						printf("Name collision for '%s' at %d (with "
							"'%s' at %d)\n", name, n, clash, match);
						exit(EXIT_FAILURE);
					}
				} else {
					// Again, unique only, stored after the originals
					sortedHash[MAX_ENTRIES + n] = hash;
					validName++;
				}
				processed++;
			} else {
				printf("Empty string at index %d\n", n);
				exit(EXIT_FAILURE);
			}
		} else {
			break;
		}
	}
	// Sort the filled unique list into ascending order
	qsort(sortedHash, MAX_ENTRIES * 2, sizeof(uint32_t), compareU32);
	// All done!
	printf("All known extensions: %d\n", processed);
	printf("%d unique, %d unprefixed unique (%d total)\n",
		validFull, validName, validFull + validName);
	printf("%d duplicates ignored (overall %d strings)\n",
		duplicate, validFull + validName + duplicate);
	puts("No collisions found!");
}

/**
 * After running \c generateHashes() this will perform sanity checks and verify
 * against the known values.
 *
 * \note This will halt and \c exit() on encountering an error.
 */
static void verifyHashes(void) {
	// The generator has already weeded out invalid entries (so all are valid)
	unsigned verified = 0;
	for (unsigned n = 0; n < MAX_ENTRIES; n++) {
		const char* ext = extension[n];
		if (ext != NULL) {
			// Test that all entries were processed
			if (fullHash[n] == 0) {
				printf("Missing full hash at %d ('%s')\n", n, ext);
				exit(EXIT_FAILURE);
			}
			if (nameHash[n] == 0) {
				printf("Missing name hash at %d ('%s')\n", n, ext);
				exit(EXIT_FAILURE);
			}
			// Verify the known good values
			if (fullHash[n] != precalcFull[n]) {
				printf("Precalculated full hash mismatch at %d ('%s') \n", n, ext);
				exit(EXIT_FAILURE);
			}
			if (nameHash[n] != precalcName[n]) {
				printf("Precalculated name hash mismatch at %d ('%s') \n", n, ext);
				exit(EXIT_FAILURE);
			}
			verified++;
		} else {
			break;
		}
	}
	printf("Extensions verified: %d\n", verified);
	// Test that no duplicates got through
	unsigned unique = 0;
	for (unsigned n = 0; n < MAX_ENTRIES * 2; n++) {
		uint32_t sortedN = sortedHash[n];
		if (sortedN != 0) {
			for (unsigned i = 0; i < MAX_ENTRIES * 2; i++) {
				if (n != i) {
					if (sortedN == sortedHash[i]) {
						printf("Duplicate unique entries: 0x%08X\n", sortedN);
						exit(EXIT_FAILURE);
					}
				}
			}
			unique++;
		}
	}
	printf("Unique hashes: %d\n", unique);
	// Test that all the zeroes are up-front
	unsigned zeroes = 0;
	for (unsigned n = 0; n < MAX_ENTRIES * 2; n++) {
		if (sortedHash[n] != 0) {
			zeroes = n;
			break;
		}
	}
	if (zeroes != MAX_ENTRIES * 2 - unique) {
		puts("Sorted entries does not have all zeroes upfront");
		exit(EXIT_FAILURE);
	}
	// Test that the binary search finds each entry
	for (unsigned n = 0; n < MAX_ENTRIES; n++) {
		const char* ext = extension[n];
		if (ext != NULL) {
			if (!hasUnique(ext, zeroes)) {
				printf("Extension not found: %s\n", ext);
				exit(EXIT_FAILURE);
			}
			if (!hasUnique(unprefix(ext, strlen(ext)), zeroes)) {
				printf("Name not found: %s\n", ext);
				exit(EXIT_FAILURE);
			}
		} else {
			break;
		}
	}
	puts("Verification passed!");
}

/**
 * Performs the work of writing out both full and name-only hash tables.
 */
static void printHashTables(void) {
	puts("// Full hashes");
	for (unsigned n = 0; n < MAX_ENTRIES && extension[n] != NULL; n++) {
		printf("0x%08X,%c", fullHash[n], spaceOrNewline(n, 8));
	}
	puts("");
	puts("// Name hashes");
	for (unsigned n = 0; n < MAX_ENTRIES && extension[n] != NULL; n++) {
		printf("0x%08X,%c", nameHash[n], spaceOrNewline(n, 8));
	}
	puts("");
}

/**
 * Performs the work of writing out the sorted unique hashes.
 */
static void printSortedHashes(void) {
	// Print the non-zero ones
	puts("// Unique hashes");
	unsigned count = 0;
	for (unsigned n = 0; n < MAX_ENTRIES * 2; n++) {
		if (sortedHash[n] != 0) {
			printf("0x%08X,%c", sortedHash[n], spaceOrNewline(count++, 8));
		}
	}
	puts("");
}

/**
 * Standalone test of the embedded Khronos extension strings.
 *
 * \note In testing this needs at least 19 bits with the 1200 or so extensions
 * to not have clashes between prefixed and unprefixed versions (approx. 2300).
 */
int main(int argc, char* argv[]) {
	bool chatty = false; // Debug output
	bool tables = false; // Print the verification tables (after Khronos updates)
	bool sorted = false; // Print the sorted unique runtime table
	for (int n = 1; n < argc; n++) {
		if (!strcmp("--chatty", argv[n]) || !strcmp("-c", argv[n])) {
			chatty = true;
		} else {
			if (!strcmp("--tables", argv[n]) || !strcmp("-t", argv[n])) {
				tables = true;
			} else {
				if (!strcmp("--sorted", argv[n]) || !strcmp("-s", argv[n])) {
					sorted = true;
				} else {
					printf("Unknown option '%s', choices are:\n", argv[n]);
					puts("\t--chatty enable debug output for the tests");
					puts("\t--tables print the verification tables");
					puts("\t--sorted print the sorted, unique runtime table");
					return EXIT_FAILURE;
				}
			}
		}
	}
	generateHashes(chatty);
	if (tables) {
		printHashTables();
	} else {
		verifyHashes();
	}
	if (sorted) {
		if (tables) {
			puts("Note: unverified sorted hashes");
		}
		printSortedHashes();
	}
	return EXIT_SUCCESS;
}
