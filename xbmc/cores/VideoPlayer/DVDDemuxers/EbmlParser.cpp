#include "EbmlParser.h"

bool EbmlReadId(CDVDInputStream *input, EbmlId *output)
{
  uint8_t first, byte, mask;
  EbmlId id;
  if (!input->Read(&first, 1))
    return false;
  id = first;
  for (mask = 1 << 7; mask > (1 << 4) && (first & mask) == 0; mask >>= 1)
  {
    if (!input->Read(&byte, 1))
      return false;
    id <<= 8;
    id |= byte;
  }
  if ((first & mask) == 0)
    return false;
  (*output) = id;
  return true;
}

bool EbmlReadLen(CDVDInputStream *input, uint64_t *output)
{
  uint8_t byte, len, mask;
  uint64_t result;
  if (!input->Read(&byte, 1))
    return false;
  if (byte == 0)
    return false;
  for (len = 1, mask = 1 << 7; len < 8 && (byte & mask) == 0; ++len, mask >>= 1);
  result = byte & ~mask;
  for (; len > 0; --len)
  {
    if (!input->Read(&byte, 1))
      return false;
    result <<= 8;
    result |= byte;
  }
  (*output) = result;
  return true;
}

bool EbmlReadUint(CDVDInputStream *input, uint64_t *output, uint64_t len)
{
  uint8_t byte;
  uint64_t result;
  for (uint64_t i = 0; i < len; ++i)
  {
    if (!input->Read(&byte, 1))
      return false;
    result = (result << 8) | byte;
  }
  (*output) = result;
  return true;
}

bool EbmlReadString(CDVDInputStream *input, std::string *output, uint64_t len)
{
  if (!EbmlReadRaw(input, output, len))
    return false;
  output->resize(output->find('\0'));
  return true;
}

bool EbmlReadRaw(CDVDInputStream *input, std::string *output, uint64_t len)
{
  std::string result(len, '\0');
  if (!input->Read(reinterpret_cast<uint8_t*>(const_cast<char*>(result.data())), len))
    return false;
  (*output) = std::move(result);
  return true;
}

EbmlId EbmlReadId(CDVDInputStream *input)
{
  EbmlId id = EBML_ID_INVALID;
  EbmlReadId(input, &id);
  return id;
}

uint64_t EbmlReadLen(CDVDInputStream *input)
{
  uint64_t len = -1;
  EbmlReadLen(input, &len);
  return len;
}

uint64_t EbmlReadUint(CDVDInputStream *input, uint64_t len)
{
  uint64_t result = -1;
  EbmlReadUint(input, &result, len);
  return result;
}

EbmlParserFunctor BindEbmlStringParser(std::string *output, uint64_t maxLen)
{
  return [output,maxLen](CDVDInputStream *input, uint64_t tagLen)
  {
    return EbmlReadString(input, output, std::min(tagLen, maxLen));
  };
}

EbmlParserFunctor BindEbmlRawParser(std::string *output, uint64_t maxLen)
{
  return [output,maxLen](CDVDInputStream *input, uint64_t tagLen)
  {
    return EbmlReadRaw(input, output, std::min(tagLen, maxLen));
  };
}

bool EbmlParseMaster(CDVDInputStream *input, const ParserMap &parserMap, uint64_t tagLen, bool stopOnError)
{
  int64_t tagEnd = input->Seek(0, SEEK_CUR) + tagLen;
  int error = 0;
  while (!input->IsEOF() && input->Seek(0, SEEK_CUR) < tagEnd)
  {
    EbmlId id = EbmlReadId(input);
    uint64_t len = EbmlReadLen(input);
    int64_t subTagEnd = input->Seek(0, SEEK_CUR) + len;
    auto parser = parserMap.find(id);
    if (parser != parserMap.end())
    {
      if (!parser->second)
        break;
      else if (!parser->second(input, len))
      {
        ++error;
        if (stopOnError)
          break;
      }
    }
    input->Seek(subTagEnd, SEEK_SET);
  }
  input->Seek(tagEnd, SEEK_SET);
  return !error;
}

bool EbmlMasterParser::operator()(CDVDInputStream *input, uint64_t tagLen)
{
  bool result = EbmlParseMaster(input, this->parser, tagLen, this->stopOnError);
  if (this->parsed)
    return this->parsed();
  return result;
}

EbmlMasterParser BindEbmlHeaderParser(EbmlHeader *ebmlHeader)
{
  EbmlMasterParser master;
  master.parser[EBML_ID_EBMLVERSION] = BindEbmlUintParser(&ebmlHeader->version);
  master.parser[EBML_ID_DOCTYPE] = BindEbmlStringParser(&ebmlHeader->doctype);
  return master;
}

bool EbmlHeader::Parse(CDVDInputStream *input)
{
  uint64_t len;
  if (EbmlReadId(input) != EBML_ID_HEADER)
    return false;
  if (!EbmlReadLen(input, &len))
    return false;
  return BindEbmlHeaderParser(this)(input, len);
}

// vim: ts=2 sw=2 expandtab
