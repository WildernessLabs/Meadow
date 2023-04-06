#include "Uuid.hpp"

Uuid16 Uuid16::PrimaryService = Uuid16((uint16_t)0x2800);
Uuid16 Uuid16::CharacteristicDeclaration = Uuid16((uint16_t)0x2803);

Uuid16::Uuid16()
{
}

Uuid16::Uuid16(uint16_t value)
{
    memcpy((void*)data, &value, 2);
}

Uuid16::Uuid16(uint8_t *other)
{
    memcpy((void*)data, other, 2);
}

// converts a single hex char to a number (0 - 15)
unsigned char hexDigitToChar(char ch)
{
	// 0-9
	if (ch > 47 && ch < 58)
		return ch - 48;

	// a-f
	if (ch > 96 && ch < 103)
		return ch - 87;

	// A-F
	if (ch > 64 && ch < 71)
		return ch - 55;

	return 0;
}

bool isValidHexChar(char ch)
{
	// 0-9
	if (ch > 47 && ch < 58)
		return true;

	// a-f
	if (ch > 96 && ch < 103)
		return true;

	// A-F
	if (ch > 64 && ch < 71)
		return true;

	return false;
}

// converts the two hexadecimal characters to an unsigned char (a byte)
unsigned char hexPairToChar(char a, char b)
{
	return hexDigitToChar(a) * 16 + hexDigitToChar(b);
}

Uuid128::Uuid128()
{
    
}

Uuid128::Uuid128(uint8_t *other)
{
    memcpy((void*)data, other, 16);
}

std::string Uuid128::ToString() const
{
	char one[10], two[6], three[6], four[6], five[14];

	snprintf(one, 10, "%02x%02x%02x%02x",
		data[15], data[14], data[13], data[12]);
	snprintf(two, 6, "%02x%02x",
		data[11], data[10]);
	snprintf(three, 6, "%02x%02x",
		data[9], data[8]);
	snprintf(four, 6, "%02x%02x",
		data[7], data[6]);
	snprintf(five, 14, "%02x%02x%02x%02x%02x%02x",
		data[5], data[4], data[3], data[2], data[1], data[0]);
	const std::string sep("-");
	std::string out(one);

	out += sep + two;
	out += sep + three;
	out += sep + four;
	out += sep + five;

	return out;
}

// create a guid from string
Uuid128::Uuid128(const char* fromString)
{
	char charOne = '\0';
	char charTwo = '\0';
	bool lookingForFirstChar = true;
	unsigned nextByte = 0;

    auto len = strlen(fromString);

	for (int i = len - 1 ; i >= 0 ; i--)
	{
        char ch = fromString[i];

		if (ch == '-')
			continue;

		if (nextByte >= 16 || !isValidHexChar(ch))
		{
			// Invalid string so bail
			memset(data, 0, 16);
			return;
		}

		if (lookingForFirstChar)
		{
			charOne = ch;
			lookingForFirstChar = false;
		}
		else
		{
			charTwo = ch;
			auto byte = hexPairToChar(charTwo, charOne);
			data[nextByte++] = byte;
			lookingForFirstChar = true;
		}
	}

	// if there were fewer than 16 bytes in the string then guid is bad
	if (nextByte < 16)
	{
        memset(data, 0, 16);
		return;
	}
}