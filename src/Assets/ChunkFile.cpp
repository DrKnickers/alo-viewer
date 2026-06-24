#include "Assets/ChunkFile.h"
#include "General/Exceptions.h"
#include "General/ExactTypes.h"
#include <cassert>
using namespace Alamo;
using namespace std;

#pragma pack(1)
struct CHUNKHDR
{
	uint32_t type;
	uint32_t size;
};

struct MINICHUNKHDR
{
	uint8_t type;
	uint8_t size;
};
#pragma pack()

ChunkType ChunkReader::nextMini()
{
	assert(m_curDepth >= 0);
	assert(m_size >= 0);

	if (m_miniSize >= 0)
	{
		// We're in a mini chunk, so skip it
		skip();
	}

	if (m_file->tell() == m_offsets[m_curDepth])
	{
		// We're at the end of the current chunk, move up one
		m_curDepth--;
		m_size     = -1;
		m_position =  0;
		return -1;
	}

	MINICHUNKHDR hdr;
	if (m_file->read((void*)&hdr, sizeof(MINICHUNKHDR)) != sizeof(MINICHUNKHDR))
	{
		throw ReadException();
	}

	m_miniSize   = letohl(hdr.size);
	m_miniOffset = m_file->tell() + m_miniSize;
	// The mini-chunk size is a single byte, but nothing constrains it to the
	// bytes actually remaining in the enclosing chunk. A corrupt/crafted .alo
	// can declare a mini-chunk that runs past its parent's end, after which a
	// later read()/skip() would over-read adjacent data. Bound the mini-chunk
	// by its parent and reject the file. (This is the read-path analogue of
	// max2alamo's writer-side size-byte guard.)
	if (m_miniOffset > m_offsets[m_curDepth])
	{
		throw BadFileException();
	}
	m_position   = 0;

	return letohl(hdr.type);
}

ChunkType ChunkReader::next()
{
	assert(m_curDepth >= 0);

	if (m_size >= 0)
	{
		// We're in a data chunk, so skip it
		skip();
	}
	
	if (m_file->tell() == m_offsets[m_curDepth])
	{
		// We're at the end of the current chunk, move up one
		m_curDepth--;
		m_size     = -1;
		m_position =  0;
		return -1;
	}

	CHUNKHDR hdr;
	if (m_file->read((void*)&hdr, sizeof(CHUNKHDR)) != sizeof(CHUNKHDR))
	{
		throw ReadException();
	}

	unsigned long size = letohl(hdr.size);
	// Guard the fixed m_offsets[MAX_CHUNK_DEPTH] array: a crafted .alo with
	// chunks nested past depth 255 would otherwise write out of bounds via
	// the pre-increment below (CWE-787 memory corruption during parse).
	// Reject the file. (nextMini() uses the flat m_miniOffset and is not
	// affected.)
	if (m_curDepth + 1 >= MAX_CHUNK_DEPTH)
	{
		throw BadFileException();
	}
	m_offsets[ ++m_curDepth ] = m_file->tell() + (size & 0x7FFFFFFF);
	m_size     = (~size & 0x80000000) ? size : -1;
	m_miniSize = -1;
	m_position = 0;

	return letohl(hdr.type);
}

void ChunkReader::skip()
{
	if (m_miniSize >= 0)
	{
		m_file->seek(m_miniOffset);
	}
	else
	{
		m_file->seek(m_offsets[m_curDepth--]);
        m_size     = -1;
        m_position =  0;
	}
}

size_t ChunkReader::size()
{
	return (m_miniSize >= 0) ? m_miniSize : m_size;
}

string ChunkReader::readString()
{
	long n = size();
	Buffer<char> data(n / sizeof(char));
	read(data, n);
	// The payload length is known (n). Do NOT build the string by scanning for a NUL:
	// some writers (e.g. headless material editors like alo_material) emit length-exact
	// strings with no trailing NUL, and `string((char*)data)` would then over-read past
	// the buffer into adjacent memory (garbage trailing byte). Bound by n, and trim any
	// trailing NUL(s) so vanilla NUL-terminated strings still come out clean.
	const char* s = (const char*)data;
	size_t len = (n > 0) ? (size_t)n : 0;
	while (len > 0 && s[len - 1] == '\0') --len;
	return string(s, len);
}

wstring ChunkReader::readWideString()
{
	Buffer<wchar_t> data(size() / sizeof(wchar_t));
	read(data, size());
	return (wchar_t*)data;
}


float ChunkReader::readFloat()
{
	float value;
    if (read(&value, sizeof(value)) < sizeof(value)) {
        throw ReadException();
    }
	return value;
}

Vector3 ChunkReader::readVector3()
{
    Vector3 out;
    out.x = readFloat();
    out.y = readFloat();
    out.z = readFloat();
    return out;
}

Vector4 ChunkReader::readVector4()
{
    Vector4 out;
    out.x = readFloat();
    out.y = readFloat();
    out.z = readFloat();
    out.w = readFloat();
    return out;
}

Color ChunkReader::readColorRGB()
{
    Color out;
    out.r = readFloat();
    out.g = readFloat();
    out.b = readFloat();
    out.a = 1.0f;
    return out;
}

Color ChunkReader::readColorRGBA()
{
    Color out;
    out.r = readFloat();
    out.g = readFloat();
    out.b = readFloat();
    out.a = readFloat();
    return out;
}

unsigned char ChunkReader::readByte()
{
	uint8_t value;
    if (read(&value, sizeof(value)) < sizeof(value)) {
        throw ReadException();
    }
	return value;
}

unsigned short ChunkReader::readShort()
{
	uint16_t value;
    if (read(&value, sizeof(value)) < sizeof(value)) {
        throw ReadException();
    }
	return letohs(value);
}

unsigned long ChunkReader::readInteger()
{
	uint32_t value;
    if (read(&value, sizeof(value)) < sizeof(value)) {
        throw ReadException();
    }
	return letohl(value);
}

size_t ChunkReader::read(void* buffer, size_t size, bool check)
{
	if (m_size >= 0)
	{
		size_t s = m_file->read(buffer, min(m_position + (long)size, (long)this->size()) - m_position);
		m_position += (long)s;
		if (check && s != size)
		{
			throw ReadException();
		}
		return size;
	}
	throw ReadException();
}

ChunkReader::ChunkReader(ptr<IFile> file)
    : m_file(file)
{
	m_offsets[0] = (unsigned long)m_file->size();
	m_curDepth   = 0;
	m_size       = -1;
	m_miniSize   = -1;
}

void ChunkWriter::beginChunk(ChunkType type)
{
	m_curDepth++;
	m_chunks[m_curDepth].offset   = m_file->tell();
	m_chunks[m_curDepth].hdr.type = type;
	m_chunks[m_curDepth].hdr.size = 0;
	if (m_curDepth > 0)
	{
		// Set 'container' bit in parent chunk
		m_chunks[m_curDepth-1].hdr.size |= 0x80000000;
	}

	// Write dummy header
	CHUNKHDR hdr = {0,0};
	m_file->write(&hdr, sizeof(CHUNKHDR));
}

void ChunkWriter::beginMiniChunk(ChunkType type)
{
	assert(m_curDepth >= 0);
	assert(m_miniChunk.offset == -1);
	assert(type <= 0xFF);

	m_miniChunk.offset   = m_file->tell();
	m_miniChunk.hdr.type = (uint8_t)type;
	m_miniChunk.hdr.size = 0;
	
	// Write dummy header
	MINICHUNKHDR hdr = {0, 0};
	m_file->write(&hdr, sizeof(MINICHUNKHDR));
}

void ChunkWriter::endChunk()
{
	assert(m_curDepth >= 0);
	if (m_miniChunk.offset != -1)
	{
		// Ending mini-chunk
		long pos  = m_file->tell();
		long size = pos - (m_miniChunk.offset + sizeof(MINICHUNKHDR));
		assert(size <= 0xFF);

		m_miniChunk.hdr.size = (uint8_t)size;
		
		MINICHUNKHDR hdr = {m_miniChunk.hdr.type, m_miniChunk.hdr.size};
		m_file->seek(m_miniChunk.offset);
		m_file->write(&hdr, sizeof(MINICHUNKHDR) );
		m_file->seek(pos);
		m_miniChunk.offset = -1;
	}
	else
	{
		// Ending normal chunk
		long pos  = m_file->tell();
		long size = pos - (m_chunks[m_curDepth].offset + sizeof(CHUNKHDR));

		CHUNKHDR hdr = { htolel(m_chunks[m_curDepth].hdr.type), htolel(m_chunks[m_curDepth].hdr.size) };
		m_chunks[m_curDepth].hdr.size = (m_chunks[m_curDepth].hdr.size & 0x80000000) | (size & ~0x80000000);
		m_file->seek(m_chunks[m_curDepth].offset);
		m_file->write(&hdr, sizeof(CHUNKHDR));
		m_file->seek(pos);

		m_curDepth--;
	}
}

void ChunkWriter::write(const void* buffer, size_t size)
{
	assert(m_curDepth >= 0);
	if (m_file->write(buffer, size) != size)
	{
		throw WriteException();
	}
}

void ChunkWriter::writeString(const std::string& str)
{
	write(str.c_str(), (int)str.length() + 1);
}

ChunkWriter::ChunkWriter(ptr<IFile> file)
    : m_file(file)
{
	m_curDepth = -1;
	m_miniChunk.offset = -1;
}
