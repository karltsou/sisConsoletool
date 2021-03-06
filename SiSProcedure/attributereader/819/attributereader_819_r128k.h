#ifndef ATTRIBUTEREADER_819_R128K_H
#define ATTRIBUTEREADER_819_R128K_H

#include "attributereader/817/attributereader_817_r64k.h"

namespace SiS
{
namespace Procedure
{

class AttributeReader_819_r128k : public AttributeReader_817_r64k
{
public:
    explicit AttributeReader_819_r128k(SiSDeviceIO* sisDeviceIO);
    explicit AttributeReader_819_r128k(unsigned char* binBuf, int binSize);
    virtual ~AttributeReader_819_r128k();

    virtual IAddressProvider* getAddressProvider();
    virtual std::string getAddressProviderTableName();
    virtual int mappingChipAddress(int chipIndex);
};

} // Procedure
} // SiS

#endif // ATTRIBUTEREADER_819_R128K_H
