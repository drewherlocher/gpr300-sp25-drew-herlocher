#pragma once


namespace dh {

	unsigned int loadHeightMap(const char* filePath);
	unsigned int loadHeightMap(const char* filePath, int wrapMode, int magFilter, int minFilter, bool mipmap);
}	