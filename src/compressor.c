//FILE: compressor.c 
//AUTHOR: Craig
//PURPOSE: contains all functions used for compression/decompression and other utilities

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <dirent.h>
#include <math.h>
#include <assert.h>
#include <float.h>
#include "compressor.h"

struct floatSplitValue { //Struct to represent the before and after decimal point values of a float split (3 bytes for each)
	uint8_t beforeDecimal[3];
	uint8_t afterDecimal[3];
};

/* 
 * Purpose:  
 * 		Get a array of the absolute filepaths of a certain type of file within a directory.
 * Parameters: 
 * 		1. files - A blank char array to hold the absolute file paths desired.
 * 		2. baseDirectory - The absolute path of the base directory that the target files are in.
 * 		3. fileExtension - The filename extension of the targeted files in the base directory.
 *		4. count - A blank pointer which later getss set to the number of files found.
 */
void getAbsoluteFilepaths(char *files[], char *baseDirectory, char *fileExtension, unsigned int *count) {
	struct dirent *directoryEntry;
	DIR *directory = opendir(baseDirectory);
	int pathLen = strlen(baseDirectory)+1; //For null char
	int i = 0;
	
	if(directory != NULL) {
		while((directoryEntry = readdir(directory)) != NULL) { //Whilst there's still entries
			if(strstr(directoryEntry->d_name, fileExtension) != NULL) { //If the entry is of the desired extension
				files[i] = calloc((strlen(directoryEntry->d_name) + pathLen), sizeof(char));
				strcat(files[i], baseDirectory);
				strcat(files[i], directoryEntry->d_name);
				i++;
			}
		}
		*count = i;
	} 
}

/*
 * Purpose:
 * 		Extract a list of floats from a given file. This method is used to get the simulation data from data dump files. (/simulation datasets/)
 * Returns:
 * 		An array of floats representing data in the file.
 * Parameters:
 * 		1. absFilePath - Absolute file path of the file that data will be extracted from.
 * 		2. count - Blank pointer passed in to be assigned to the number of indexes in the returned array.
 *		3. max - Blank pointer passed in to be assigned to the maximum value in the returned array.
 *		4. min - Blank pointer passed in to be assigned to the minimum value in the returned array.
 *		5. mean - Blank pointer passed in to be assigned to the average value of the returned array.
 */
float *getData(char *absFilePath, unsigned int *count, float *max, float *min, float *mean) {
	FILE *contentFile = fopen(absFilePath, "r");
	float *fileContent = malloc((150*150*150)*sizeof(float)); //arbitrarily large allocation for other datasets
	int i = 0;
	*max = FLT_MIN;
	*min = FLT_MAX;
	float total = 0;

	while(fscanf(contentFile, "%f", &fileContent[i]) == 1) { //while there's still data left, copy it into the array
		total+=fileContent[i];
		if(fileContent[i] > *max) {
			*max = fileContent[i];
		} else if(fileContent[i] < *min) {
			*min = fileContent[i];
		}
		i++;
	}
	fclose(contentFile);
	float *exactContent = malloc(i*sizeof(float)); //resize to whats needed
	memcpy(exactContent, fileContent, i*sizeof(float));
	*count = i;
	*mean = total/ *count;

	return exactContent;
}

/*
 * Purpose:
 * 		Extract a list of ints from a given file. This method is used to get ints representing values of bytes which is used for testing purposes.
 * Returns:
 * 		An array of ints representing data in the file
 * Parameters:
 * 		1. absFilePath - absolute file path of the file that data will be extracted from
 * 		2. dataLength - passed in to get the number of values/indexes of the data
 */
unsigned int *getVerificationData(char *absFilePath, unsigned int *dataLength) {
	FILE *contentFile = fopen(absFilePath, "r");
	unsigned int *fileContent = malloc(100*sizeof(unsigned int)); //arbitrarily large allocation (supports bigger test files) 
	unsigned int i = 0;

	while(fscanf(contentFile, "%d", &fileContent[i]) == 1) { //while there's still data left, copy it into the array
		i++;
	}
	fclose(contentFile);
	unsigned int *exactContent = malloc(i*sizeof(unsigned int));
	memcpy(exactContent, fileContent, i*sizeof(unsigned int)); //resize to whats needed
	*dataLength = i;

	return exactContent;
}

/*
 * Purpose:
 *		Get the number of digits required to represent an numberOfBits size number.
 * Returns:
 *		The maximum number of digits needed to represent a numBits size number. (e.g. pass numberOfBits=7, 2^7=128, 3 digits).
 * Parameters:
 *		1. numberOfBits - The number of bits a number uses
 */
unsigned int numberOfDigits (unsigned int numberOfBits) {
    if (numberOfBits == 0) return 1;
    return floor (log10 (pow(2,numberOfBits))) + 1;
}

/*
 * Purpose:
 * 		Split a float into a floatSplitValue struct that contains 2 ints for the value before and after the decimal (after value is multiplied by multiplier).
 * Returns:
 * 		A floatSplitValue struct which represents the passed float scaled up.
 * Parameters:
 * 		1. initialValue - The initial float value to be broken up.
 * 		2. multiplier - How much to multiply the value after the decimal point to. (10,100,10,000 ...).
 */
struct floatSplitValue splitFloat(float initialValue, unsigned int multiplier) {
	float beforeDp, afterDp;
	uint32_t before, after;
	afterDp = modff(initialValue, &beforeDp); //modff to get the before and after decimal as floats
	before = (uint32_t) fabs(beforeDp);
	after = (uint32_t) round((fabs(afterDp) * multiplier));

	uint8_t beforeBytes[3];
	beforeBytes[2] = (before >> 16) & 0xFF;
	beforeBytes[1] = (before >> 8) & 0xFF;
	beforeBytes[0] = before & 0xFF;

	uint8_t afterBytes[3];
	afterBytes[2] = (after >> 16) & 0xFF;
	afterBytes[1] = (after >> 8) & 0xFF;
	afterBytes[0] = after & 0xFF;

	struct floatSplitValue split = {beforeBytes[0], beforeBytes[1], beforeBytes[2], afterBytes[0], afterBytes[1], afterBytes[2]};

	return split;
}

/* 
 * Purpose:
 *		Using runlength compression, compress an array of floats to runlengthEntry structs (uses malloc because its poor performance and not targeted towards going on an accelerator)
 * Returns:
 * 		Array of runlengthEntrys that represent a runlength compressed version of the given values.
 * Parameters:
 * 		1. values - Array of float values that are to be compressed.
 * 		2. count - A count of the number of values in the given data.
 * 		3. newCount - A count of the number of entries in the runlength compressed data.
 */
struct runlengthEntry *getRunlengthCompressedData(float *allValues, unsigned int count, unsigned int *newCount) {
	struct runlengthEntry *compressedData = calloc(count, sizeof(struct runlengthEntry));
	compressedData[0].value = allValues[0]; //Have to initilise the first index then work from there
	compressedData[0].valueCount = 1;
	int uci, ci = 0; //ci = compressed array index, uci = uncompressed array index

	for(uci=1; uci < count; uci++) {
		if(compressedData[ci].value == allValues[uci]) { //If value is continued
			compressedData[ci].valueCount++;
		} else { //New value, create an entry to store it
			ci++;
			compressedData[ci].value = allValues[uci];
			compressedData[ci].valueCount = 1;
		}
	}
	*newCount =  ci+1;
	return compressedData;
}

/*
 * Purpose:
 * 		Decompress a runlength compressed set of floats into the original array of floats.
 * Returns:
 * 		Array of floats represented the uncompressed version of the given data.
 * Parameters:
 * 		1. compressedValues - a array of runlengthEntry structs representing a lsit of runlength compressed floats
 * 		2. count - the number of runlengthEntry structs that compressedValues contains
 * 		3. newCount - blank pointer that gets assigned the number of entries in the returned decompressed data
 */
float *getRunlengthDecompressedData(struct runlengthEntry *compressedValues, unsigned int count, unsigned int *newCount) {
	unsigned int i;
	unsigned int totalCount = 0;

	for(i = 0; i < count; i++) { //find out the number of elements (if decompressed) compressedValues contains
		totalCount+= compressedValues[i].valueCount;
	}
	float *uncompressedValues = malloc(totalCount*sizeof(float));
	
	unsigned int newPos = 0;
	for(i = 0; i < count; i++) {
		while(compressedValues[i].valueCount != 0) { //"unwrap" the current entry and create elements for it
			uncompressedValues[newPos] = compressedValues[i].value;
			newPos++;
			compressedValues[i].valueCount--;
		}
	}
	*newCount = newPos;
	return uncompressedValues;
}

/*
 * Purpose:
 * 		Compress the given data into a 24 bit format using the given parameters to cut down the original data.
 * Returns:
 * 		Array of compressedVal (24 bit) values representing a compressed version the original array of 32 bit floats.
 * Parameters:
 * 		1. uncompressedData - List of 32 bit floats to be compressed
 *		2. count - The number of values in the parameter 1.
 * 		2. magBits - Number of bits to be used to represent the magnitude of the data (bits used for before decimal place).
 *		3. precBits - Number of bits to be used to represent the precision of the data (bits used for after decimal place).
 */
struct compressedVal *get24BitCompressedData(float *uncompressedData, unsigned int count, unsigned int magBits, unsigned int precBits) {
	struct compressedVal *compressedData = calloc(count, sizeof(struct compressedVal)); //Create array for new compressed values, dynamic allocation since this part shouldnt be run on accelerator
	unsigned int multiplier;

	//multiplier for value after decimal
	if(numberOfDigits(precBits) == 1) {
		multiplier = 10;
	} else {
		multiplier = pow(10, numberOfDigits(precBits)-1); //max number of digits that can be represented by a precBits number
	}
	
	unsigned int i, space, target, ci, uci; //ci = compressed index, uci = uncompressed indexspace is byte space, target is number of bits trying to move
	struct floatSplitValue value;

	for(i = 0; i < count; i++) {
		value = splitFloat(uncompressedData[i], multiplier); //turn float into 2 ints representing before and after decimal

		//extracting sign bit
		if (uncompressedData[i] < 0) {
			compressedData[i].data[2] = 1 << 7;
		}
		if(magBits + 1 == 8) { //Case with 7 bits of magnitude, 16 of precision
			compressedData[i].data[2] = compressedData[i].data[2] | value.beforeDecimal[0];
			compressedData[i].data[1] = value.afterDecimal[1];
			compressedData[i].data[0] = value.afterDecimal[0];
		} 
		else if(magBits + 1 < 8) { //Case where some precision bits are in byte 2 (small mag, high prec)
			compressedData[i].data[2] = compressedData[i].data[2] | (value.beforeDecimal[0] << 7-magBits);
			compressedData[i].data[2] = compressedData[i].data[2] | value.afterDecimal[2];
			compressedData[i].data[1] = value.afterDecimal[1];
			compressedData[i].data[0] = value.afterDecimal[0];
		} else if(magBits + 1 > 8) { //Case where magnitude takes up 8 or more bits
			space = 7; //8-sign
			ci = 2; //start on LHS byte of compressed struct

			if(magBits >= 17) { //magnitude all 3 bytes
				uci = 2;
				target = magBits-16;
			} else if(magBits >= 9) { // magnitude in byte 2
				uci = 1;
				target = magBits - 8;
			} else { //magnitude in byte 1
				uci = 0;
				target = magBits;
			}

			while (target != 0) {	//shift in magnitude bits
				if (space >= target) { //If we can fit the current values byte into the compressed byte, we want RHS of our data
					compressedData[i].data[ci] = compressedData[i].data[ci] | (value.beforeDecimal[uci] << space-target);
					space = space - target;
					if(space == 0) { //If there's no space left, move to the next compressed byte
						ci--;
						space = 8;
					}
					if(uci > 0) { //If we're not on our last byte, decrement
						uci--;
						target = 8;
					} else { //Done, get out of while
						uci--;
						target = 0;
					}
				} else { //We're trying to squeeze more data than we have free indexes, we want LHS of our data
					compressedData[i].data[ci] = compressedData[i].data[ci] | (value.beforeDecimal[uci] >> target-space);
					ci--;
					target = target-space;
					space = 8;
				}
			}
			if(magBits >= 17) { //shift in precision
				compressedData[i].data[0] = compressedData[i].data[0] | (value.afterDecimal[0] & ((uint32_t) pow(2, precBits) - 1));
			} else {
				compressedData[i].data[1] = compressedData[i].data[1] | value.afterDecimal[1];
				compressedData[i].data[0] = value.afterDecimal[0];
			}
		}
	}
	return compressedData;
}

/*
 * Purpose:
 * 		Decompress the 24 bit format data array into a version of the original data with some precision lost, depending on magnitude and precision sizes.
 * Returns:
 * 		Array of floats representing the data contained in the 24 bit format.
 * Parameters:
 * 		1. allValues - An array of 24 bit compressed values.
 *		2. count - The number of 24 bit compressed values in parameter allValues
 * 		2. magBits - Number of bits that have been used to represent magnitude in the 24 bit notation.
 *		3. precBits - Number of bits that have been used to represent precision in the 24 bit notation.
 */
float *get24BitDecompressedData(struct compressedVal *allValues, unsigned int count, unsigned int magBits, unsigned int precBits) {
	float *uncompressed = calloc(count, sizeof(float));
	unsigned int divider;
	if(numberOfDigits(precBits) == 1) {
		divider = 10;
	} else {
		divider = pow(10, numberOfDigits(precBits)-1);
	}
	unsigned int beforeDp = 0;
	unsigned int afterDp = 0;
	int i, signMultiplier;

	for(i = 0; i < count; i++) {
		beforeDp = 0;
		afterDp = 0;

		if((allValues[i].data[2] >> 7) == 1) { //if sign bit is set then it's a -ve number represented 
			signMultiplier = -1;
		} else {
			signMultiplier = 1;
		}
		if(magBits + 1 < 8) { //Magnitude in byte 2 as well as precision and precision in byte 1 and 0
			beforeDp = (allValues[i].data[2] >> (7-magBits)) & (uint32_t) (pow(2, magBits)-1); //Shift off the extra precision then AND to keep bits of interest
			afterDp = (allValues[i].data[2] & (uint32_t) (pow(2,(precBits % 8))-1)) << 16; //use AND to keep the number of bits on RHS we want for precision and shift into place
			afterDp = afterDp | (allValues[i].data[1] << 8);
			afterDp = afterDp | allValues[i].data[0];
		} else if(magBits + 1 == 8) { //Magnitude and sign bit fill up byte 2, precision in other 2 bytes
			beforeDp = allValues[i].data[2] & (uint32_t) (pow(2, magBits)-1); //Extract RHS 7 bits we want and take precision
			afterDp = allValues[i].data[1] << 8;
			afterDp = afterDp | allValues[i].data[0];
		} else if(magBits+1 > 8) { //Magnitude fills over a byte and more
			beforeDp = (allValues[i].data[2] & (((uint32_t) pow(2,7)) - 1)) << (magBits - 7); //Extract last 7 bits from the first byte
			int magBitsLeft = magBits - 7;
			int precBitsLeft = precBits;
			int ci = 1; //compressed data index

			while(magBitsLeft != 0) {
				if(magBitsLeft >= 8) { //Whole byte or more of interest
					beforeDp = beforeDp | (allValues[i].data[ci] << (magBitsLeft-8));
					magBitsLeft = magBitsLeft - 8;
					ci--;
				} else { //LHS of byte of interest, use masks to extract relevant bits
					beforeDp = beforeDp | ((((uint32_t) pow(2,8)-1) - ((uint32_t) pow(2,8-magBitsLeft)-1)) & allValues[i].data[ci]) >> (8-magBitsLeft);
					magBitsLeft = 0;
				}
			}
			while(precBitsLeft != 0) {
				if(precBitsLeft >= 8) { //Precision is the whole byte
					afterDp = afterDp | allValues[i].data[ci];
					precBitsLeft = precBitsLeft - 8;
				} else { //Precision is on RHS and part on LHS is to be ignored
					afterDp = ((uint32_t) pow(2, precBits)-1) & allValues[i].data[ci];
					ci--;
					precBitsLeft = 0;
				}
			}
		}
		uncompressed[i] = signMultiplier * (beforeDp + ((float) afterDp) / divider);
	}
	return uncompressed;
}

/*
 * Purpose:
 *		Retrieve and decompress a single value from an array of 24 bit compressed values.
 * Returns:
 *		Value converted back to float, possibly with precision lost.
 * Parameters:
 *		1. allValues - An array of 24 bit compressed values
 *		2. count - The number of 24 bit compressed values in allValues
 * 		2. magBits - Number of bits that have been used to represent magnitude in the 24 bit notation.
 *		3. precBits - Number of bits that have been used to represent precision in the 24 bit notation.
 */
float getSingle24BitValue(struct compressedVal *allValues, unsigned int index, unsigned int magBits, unsigned int precBits) {
	unsigned int beforeDp = 0;
	unsigned int afterDp = 0;
	unsigned int divider;
	int signMultiplier = 0;

	//setup value to divide integer for after decimal
	if(numberOfDigits(precBits) == 1) {
		divider = 10;
	} else {
		divider = pow(10, numberOfDigits(precBits)-1);
	}

	struct compressedVal targetVal = allValues[index];

	//sign bit extraction
	if(targetVal.data[2] >> 7 == 1) {
		signMultiplier = -1;
	} else {
		signMultiplier = 1;
	}

	if(magBits + 1 < 8) { //Magnitude in byte 2 as well as precision and precision in byte 1 and 0
			beforeDp = (targetVal.data[2] >> (7-magBits)) & (uint32_t) (pow(2, magBits)-1); //Shift off the extra precision then AND to keep bits of interest
			afterDp = (targetVal.data[2] & (uint32_t) (pow(2,(precBits % 8))-1)) << 16; //use AND to keep the number of bits on RHS we want for precision and shift into place
			afterDp = afterDp | (targetVal.data[1] << 8);
			afterDp = afterDp | targetVal.data[0];
		} else if(magBits + 1 == 8) { //Magnitude and sign bit fill up byte 2, precision in other 2 bytes
			beforeDp = targetVal.data[2] & (uint32_t) (pow(2, magBits)-1); //Extract RHS 7 bits we want and take precision
			afterDp = targetVal.data[1] << 8;
			afterDp = afterDp | targetVal.data[0];
		} else if(magBits+1 > 8) { //Magnitude fills over a byte and more
			beforeDp = (targetVal.data[2] & (((uint32_t) pow(2,7)) - 1)) << (magBits - 7); //Extract last 7 bits from the first byte
			magBits = magBits - 7;
			int precBitsLeft = precBits;
			int ci = 1; //compressed data index

			while(magBits != 0) { //while magnitude bits left to decompress
				if(magBits >= 8) { //Whole byte or more of interest
					beforeDp = beforeDp | (targetVal.data[ci] << (magBits-8));
					magBits = magBits - 8;
					ci--;
				} else { //LHS of byte of interest, use masks to extract relevant bits
					beforeDp = beforeDp | ((((uint32_t) pow(2,8)-1) - ((uint32_t) pow(2,8-magBits)-1)) & targetVal.data[ci]) >> (8-magBits);
					magBits = 0;
				}
			}
			while(precBitsLeft != 0) { //while precision bits left to decompress
				if(precBitsLeft >= 8) { //Precision is the whole byte
					afterDp = afterDp | targetVal.data[ci];
					precBitsLeft = precBitsLeft - 8;
				} else { //Precision is on RHS and part on LHS is to be ignored
					afterDp = ((uint32_t) pow(2, precBits)-1) & targetVal.data[ci];
					ci--;
					precBitsLeft = 0;
				}
			}
		}
		return signMultiplier * (beforeDp + ((float) afterDp) / divider);
}

/*
 * Purpose:
 *		Compress and insert a floating point value into the array of 24 bit compressed values
 * Parameters:
 *		1. allValues - The array of 24 bit compressed values
 *		2. updated - The floating point value to be compressed and inserted to allValues
 *		3. index - The index the new value is to override
 * 		4. magBits - Number of bits that have been used to represent magnitude in the 24 bit notation.
 *		5. precBits - Number of bits that have been used to represent precision in the 24 bit notation.
 */
void insertSingle24BitValue(struct compressedVal *allValues, float updatedValue, unsigned int index, unsigned int magBits, unsigned int precBits) {
	//clear existing bytes
	allValues[index].data[2] = 0;
	allValues[index].data[1] = 0;
	allValues[index].data[0] = 0;

	unsigned int multiplier;
	if(numberOfDigits(precBits) == 1) {
		multiplier = 10;
	} else {
		multiplier = pow(10, numberOfDigits(precBits)-1); //max number of digits that can be represented by a precBits number
	}
	unsigned int i, space, target, ci, uci; //ci = compressed index, uci = uncompressed indexspace is byte space, target is number of bits trying to move
	struct floatSplitValue value = splitFloat(updatedValue, multiplier); //turn float into 2 ints representing before and after decimal

	if (updatedValue < 0) { //Set the sign bit (1 for -ve, 0 for +ve)
			allValues[index].data[2] = 1 << 7;
		}
		if(magBits + 1 == 8) { //Case with 7 bits of magnitude, 16 of precision
			allValues[index].data[2] = allValues[index].data[2] | value.beforeDecimal[0];
			allValues[index].data[1] = value.afterDecimal[1];
			allValues[index].data[0] = value.afterDecimal[0];
		} 
		else if(magBits + 1 < 8) { //Case where some precision bits are in byte 2 (small mag, high prec)
			allValues[index].data[2] = allValues[index].data[2] | (value.beforeDecimal[0] << (7-magBits));
			allValues[index].data[2] = allValues[index].data[2] | value.afterDecimal[2];
			allValues[index].data[1] = value.afterDecimal[1];
			allValues[index].data[0] = value.afterDecimal[0];
		} else if(magBits + 1 > 8) { //Case where magnitude takes up 8 or more bits
			space = 7; //8-sign
			ci = 2; //start on LHS byte of compressed struct

			if(magBits >= 17) { //magnitude all 3 bytes
				uci = 2;
				target = magBits-16;
			} else if(magBits >= 9) { // magnitude in byte 2
				uci = 1;
				target = magBits - 8;
			} else { //magnitude in byte 1
				uci = 0;
				target = magBits;
			}

			while (target != 0){ //While we have bits to insert and we've got bytes to extract from
				if (space >= target) { //If we can fit the current values byte into the compressed byte
					allValues[index].data[ci] = allValues[index].data[ci] | (value.beforeDecimal[uci] << (space-target));
					space = space - target;
					if(space == 0) { //If there's no space left, move to the next compressed byte
						ci--;
						space = 8;
					}
					uci--;
					if(uci == 0) { //If we're not on our last byte, decrement;
						target = 8;
					} else { //Done, get out of while
						target = 0;
					}
				} else { //We're trying to squeeze more data than we have free indexes
					allValues[index].data[ci] = allValues[index].data[ci] | (value.beforeDecimal[uci] >> (target-space));
					ci--;
					target = target-space;
					space = 8;
				}
			}
			if(magBits >= 17) { //Extract the magnitude, either in last byte or part of byte 1 and all of last byte
				allValues[index].data[0] = allValues[index].data[0] | (value.afterDecimal[0] & (((uint32_t) pow(2, precBits)) - 1));
			} else {
				allValues[index].data[1] = allValues[index].data[1] | value.afterDecimal[1];
				allValues[index].data[0] = value.afterDecimal[0];
			}
		}
}

/*
 * Purpose:
 *		Compress the given array of floats into a potentially non-byte aligned format of the specified magnitude and precision sizes
 * Returns:
 *		Array of chars representing the compressed version of the uncompressed data passed.
 * Parameters:
 *		1. uncompressedData - The array of floats to be compressed
 *		2. count - The number of elements in uncompressedData
 *		3. newCount - The number of bytes used for the new compressed representation
 * 		4. magBits - Number of bits that have been used to represent magnitude in the 24 bit notation.
 *		5. precBits - Number of bits that have been used to represent precision in the 24 bit notation.
 */
unsigned char *getVariableBitCompressedData(float *uncompressedData, unsigned int count, unsigned int *newCount, unsigned int magBits, unsigned int precBits) {
	unsigned int ci = (int) ceil(((count*(1+magBits+precBits))/(float)8))-1; //calculate starting index of the value 
	*newCount = ci+1;
	unsigned char *compressedData = calloc(ci+1, sizeof(unsigned char));

	int j;
	unsigned int space = 8;
	unsigned int target = 1;
	struct floatSplitValue value;
	unsigned int multiplier;

	if(numberOfDigits(precBits) == 1) {
		multiplier = 10;
	} else {
		multiplier = pow(10, numberOfDigits(precBits)); //max number of digits that can be represented by a precBits number
	}
	unsigned int uci = 0;
	int i;
	for(i = 0; i < count; i++) {
		target = 1;
		value = splitFloat(uncompressedData[i], multiplier);
		if(uncompressedData[i] < 0) { //need to deal with -ve sign
			compressedData[ci] = compressedData[ci] | (1 << (space-target));
		}
		space--;
		if(space==0) { //move onto next compressed byte if we have to
			ci--;
			space = 8;
		} //space 3
		if(magBits >= 17) { //magnitude is in 3 bytes
			uci = 2;
			target = magBits-16;
		} else if(magBits >=9) { //magnitude in 2 bytes
			uci = 1;
			target = magBits-8;
		} else { //magnitude only in 1
			uci = 0; //uci becomes 0
			target = magBits; //target becomes 4
		}
		while(target != 0) {//} && uci >= 0) { //deal with magnitude bits
			if(space >= target) { //If we can fit target into current compressed byte
				compressedData[ci] = compressedData[ci] | (value.beforeDecimal[uci] << (space-target));
				space = space - target;
				if(space == 0) {
					ci--;
					space = 8;
				}
				if(uci > 0) { //if not on last byte then move further down
					uci--;
					target = 8;
				} else {
					uci--;
					target = 0;
				}
			} else { //trying to deal with more bits than space
				compressedData[ci] = compressedData[ci] | (value.beforeDecimal[uci] >> (target-space)); 
				ci--;
				target = target - space;
				space = 8;
			}
		}
		if(precBits >= 17) { //magnitude is in 3 bytes
			uci = 2;
			target = precBits-16;
		} else if(precBits >=9) { //magnitude in 2 bytes
			uci = 1;
			target = precBits-8;
		} else { //magnitude only in 1
			uci = 0;
			target = precBits;
		}
		while(target != 0) {//} && uci >= 0) { //deal with magnitude bits
			if(space >= target) { //If we can fit target into current compressed byte
				compressedData[ci] = compressedData[ci] | (value.afterDecimal[uci] << (space-target));
				space = space - target;
				if(space == 0) {
					ci--;
					space = 8;
				}
				if(uci > 0) { //if not on last byte then move further down
					uci--;
					target = 8;
				} else {
					uci--;
					target = 0;
				}
			} else { //trying to deal with more bits than space
				compressedData[ci] = compressedData[ci] | (value.afterDecimal[uci] >> (target-space));
				ci--;
				target = target - space;
				space = 8;
			}
		}
	}
	return compressedData;
}

/*
 * Purpose:
 *		Decompress the given array of compressed values back into floats (precision can be lost)
 * Returns:
 *		Array of floats representing the uncompressed contents of allValues.
 * Parameters:
 *		1. allValues - The array of compressed values to be decompressed
 *		2. count - The number of bytes (items) that allValues takes up
 *		3. newCount - The number of uncompressed items in the returned value
 * 		4. magBits - Number of bits that have been used to represent magnitude in the 24 bit notation.
 *		5. precBits - Number of bits that have been used to represent precision in the 24 bit notation.
 */
float *getVariableBitDecompressedData(unsigned char *allValues, unsigned int count, unsigned int *newCount, unsigned int magBits, unsigned int precBits) {
	float *uncompressed = calloc(count, sizeof(float));
	unsigned int uci = 0; //uncompressedindex (for storing uncomp values)
	unsigned int beforeDp;
	unsigned int afterDp;
	unsigned int i;
	int signMultiplier;
	unsigned int ci = count-1; //compressed index
	unsigned int target; //how many target bits (how many bits trying to insert)
	unsigned int uncompBits = 8; //uncompressedbits
	unsigned int uncompLimit =  (uint32_t) (ceil((8*count)/(1+magBits+precBits)));
	unsigned int divider;

	if(numberOfDigits(precBits) == 1) {
		divider = 10;
	} else {
		divider = pow(10, numberOfDigits(precBits)-1);
	}
	divider = divider*10;

	//While still have bytes to decompress
	while(uci != uncompLimit) {
		beforeDp = 0;
		afterDp = 0;
		target = 1;
		if(((allValues[ci] >> (uncompBits-target)) & (uint32_t) pow(2,target)-1) == 1) { //extract sign bit
			signMultiplier = -1;
		} else {
			signMultiplier = 1;
		}
		uncompBits--;

		if(uncompBits == 0) { //if we run out of space, move onto next byte
			ci--;
			uncompBits = 8;
		}

		//deal with magnitude bits
		target = magBits; //extract magnitude
		while(target != 0) {
			if(target > uncompBits) { //trying to extract more bits than available in current byte
				//find out the number of useful bits we have (it'll be on RHS) AND to get it then move on
				beforeDp = beforeDp | (((allValues[ci]) & (uint32_t) pow(2, uncompBits)-1) << (target-uncompBits));
				ci--;
				target = target - uncompBits;
				uncompBits = 8;
			} else { //got what we want in the current byte
				beforeDp =  beforeDp | ((allValues[ci] >> (uncompBits-target) & (uint32_t) pow(2, target)-1));
				uncompBits = uncompBits - target;
				target = 0;
				if(uncompBits == 0) {
					ci--;
					uncompBits = 8;
				}
			}
		}

		//deal with precision bits
		target = precBits;
		while(target != 0) {
			if(target > uncompBits) { //trying to extract more bits than available in current byte
				//find out the number of useful bits we have (it'll be on RHS) AND to get it then move on
				afterDp = (afterDp | ((allValues[ci]) & (uint32_t) pow(2, uncompBits)-1)) << (target-uncompBits);
				ci--;
				target = target - uncompBits;
				uncompBits = 8;
			} else { //got what we want in the current byte
				afterDp = afterDp | ((((allValues[ci] >> (uncompBits-target)) & (uint32_t) pow(2, target)-1)));
				uncompBits = uncompBits - target;
				target = 0;
				if(uncompBits == 0) {
					ci--;
					uncompBits = 8;
				}
			}
		}
		uncompressed[uci] = signMultiplier * (beforeDp + ((float) afterDp) / divider);
		uci++;
	}
	return uncompressed;
}

/*
 * Purpose:
 *		Retrieve and decompress a float from the given compressed array
 * Returns:
 *		Floating point number decompressed from the array (precision can be lost)
 * Parameters:
 *		1. allValues - The array of compressed values
 *		2. byteCount - The number of bytes (items) that allValues takes up
 *		3. targetIndex - The index (in the index in uncompressed version of allValues) of the value desired.
 * 		4. magBits - Number of bits that have been used to represent magnitude in the 24 bit notation.
 *		5. precBits - Number of bits that have been used to represent precision in the 24 bit notation.
 */
float getSingleVariableBitValue(unsigned char *allValues, unsigned int byteCount, unsigned int targetIndex, unsigned int magBits, unsigned int precBits) {
	int startByte = floor(targetIndex*(1+magBits+precBits)/8);
	int startIndex = (targetIndex*(1+magBits+precBits)) % 8;
	startByte =  byteCount-1-startByte;
	startIndex = 7-startIndex;
	unsigned int divider;
	if(numberOfDigits(precBits) == 1) {
		divider = 10;
	} else {
		divider = pow(10, numberOfDigits(precBits)-1);
	}
	divider = divider*10;
	unsigned int beforeDp;
	unsigned int afterDp;
	int i, signMultiplier;
	unsigned int ci = startByte;
	unsigned int target; //how many target bits
	unsigned int uncompBits = startIndex+1; //uncompressedbits

	beforeDp = 0;
	afterDp = 0;
	target = 1;

	//deal with sign
	if(((allValues[ci] >> (uncompBits-target)) & (uint32_t) pow(2,target)-1) == 1) { //extract sign bit
		signMultiplier = -1;
	} else {
		signMultiplier = 1;
	}
	uncompBits--;
	if(uncompBits == 0) { //if we run out of space, move onto next byte
		ci--;
		uncompBits = 8;
	}

	//deal with magnitude bits
	target = magBits; //extract magnitude
	while(target != 0) {
		if(target > uncompBits) { //trying to extract more bits than available in current byte
			//find out the number of useful bits we have (it'll be on RHS) AND to get it then move on
			beforeDp = beforeDp | (((allValues[ci]) & (uint32_t) pow(2, uncompBits)-1) << (target-uncompBits));
			ci--;
			target = target - uncompBits;
			uncompBits = 8;
		} else { //got what we want in the current byte
			beforeDp =  beforeDp | ((allValues[ci] >> (uncompBits-target) & (uint32_t) pow(2, target)-1));
			uncompBits = uncompBits - target;
			target = 0;
			if(uncompBits == 0) {
				ci--;
				uncompBits = 8;
			}
		}
	}

	//deal with precision bits
	target = precBits;
	while(target != 0) {
		if(target > uncompBits) { //trying to extract more bits than available in current byte
			afterDp = (afterDp | ((allValues[ci]) & (uint32_t) pow(2, uncompBits)-1)) << (target-uncompBits);
			ci--;
			target = target - uncompBits;
			uncompBits = 8;
		} else { //got what we want in the current byte
			afterDp = afterDp | ((((allValues[ci] >> (uncompBits-target)) & (uint32_t) pow(2, target)-1)));
			uncompBits = uncompBits - target;
			target = 0;
			if(uncompBits == 0) {
				ci--;
				uncompBits = 8;
			}
		}
	}
	return signMultiplier * (beforeDp + ((float) afterDp) / divider);
}

/*
 * Purpose:
 *		Compress and insert a given float into the given compressed array
 * Parameters:
 *		1. allValues - The array of compressed values
 *		2. byteCount - The number of bytes (items) that allValues takes up
 *		3. targetIndex - The index (in the index in uncompressed version of allValues) where value is to be inserted into
 *		4. value - Floating point value to be inserted
 * 		5. magBits - Number of bits that have been used to represent magnitude in the 24 bit notation.
 *		6. precBits - Number of bits that have been used to represent precision in the 24 bit notation.
 */
void insertSingleVariableBitValue (unsigned char *allValues, unsigned int byteCount, unsigned int targetIndex, float value, unsigned int magBits, unsigned int precBits) {
	int startByte = floor(targetIndex*(1+magBits+precBits)/8);
	int startIndex = (targetIndex*(1+magBits+precBits)) % 8;
	startByte =  byteCount-1-startByte;
	startIndex = 7-startIndex;
	unsigned int ci = startByte;
	int j;
	unsigned int space = startIndex;
	unsigned int target = 1;
	struct floatSplitValue split;
	unsigned int multiplier;

	if(numberOfDigits(precBits) == 1) {
		multiplier = 10;
	} else {
		multiplier = pow(10, numberOfDigits(precBits)); //max number of digits that can be represented by a precBits number
	}
	int totalToZero = 1+magBits+precBits;
	int currentCount = startIndex;
	int ind = ci;
	while(totalToZero != 0) {

		allValues[ind] &= ~(1UL << currentCount);
		currentCount--;
		totalToZero--;
		if(currentCount == -1) {
			currentCount = 7;
			ind--;
		}
	}

	unsigned int uci = 0; //current floats index

		target = 1;
		split = splitFloat(value, multiplier);

		if(value < 0) { //need to deal with -ve sign

			allValues[ci] = allValues[ci
				] | (1 << (startIndex));
		}

		if(space==0) { //move onto next compressed byte if we have to
			ci--;
			space = 8;
		} //space 3
		if(magBits >= 17) { //magnitude is in 3 bytes
			uci = 2;
			target = magBits-16;
		} else if(magBits >=9) { //magnitude in 2 bytes
			uci = 1;
			target = magBits-8;
		} else { //magnitude only in 1
			uci = 0; //uci becomes 0
			target = magBits; //target becomes 4
		}

		//while(target != 0 && uci >= 0) { //deal with magnitude bits
		while(target != 0) {//} && uci >= 0) { //deal with magnitude bits
			if(space >= target) { //If we can fit target into current compressed byte, need to store whats behind

				allValues[ci] = allValues[ci] | (split.beforeDecimal[uci] << (space-target));

				space = space - target;
				if(space == 0) {
					ci--;
					space = 8;
				}
				if(uci > 0) { //if not on last byte then move further down
					uci--;
					target = 8;
				} else {
					uci--;
					target = 0;
				}
			} else { //trying to deal with more bits than space
				allValues[ci] = allValues[ci] | (split.beforeDecimal[uci] >> (target-space)); 
				ci--;
				target = target - space;
				space = 8;
			}
		}
		if(precBits >= 17) { //magnitude is in 3 bytes
			uci = 2;
			target = precBits-16;
		} else if(precBits >=9) { //magnitude in 2 bytes
			uci = 1;
			target = precBits-8;
		} else { //magnitude only in 1
			uci = 0;
			target = precBits;
		}
		while(target != 0) {//deal with prec bits
			if(space >= target) { //If we can fit target into current compressed byte
				allValues[ci] = ((((uint32_t)pow(2, 8)-1) - ((uint32_t)pow(2,space)-1)) & allValues[ci]);
				allValues[ci] = allValues[ci] | (split.afterDecimal[uci] << (space-target));
				space = space - target;
				if(space == 0) {
					ci--;
					space = 8;
				}
				if(uci > 0) { //if not on last byte then move further down
					uci--;
					target = 8;
				} else {
					uci--;
					target = 0;
				}
			} else { //trying to deal with more bits than space
				allValues[ci] = ((((uint32_t)pow(2, 8)-1) - ((uint32_t)pow(2,space)-1)) & allValues[ci]);
				allValues[ci] = allValues[ci] | (split.afterDecimal[uci] >> (target-space));
				ci--;
				target = target - space;
				space = 8;
			}
		}
}