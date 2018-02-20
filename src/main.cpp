//
//  main.cpp
//  
//
//  Created by Colin Cantrell on 1/2/18.
//
//

#include "Util/include/debug.h"
#include "Util/include/runtime.h"
#include "LLD/templates/sector.h"
#include "LLC/hash/SK.h"
#include "LLC/include/random.h"

#include "leveldb/db.h"

#include <db_cxx.h>

class CBlock
{
public:
    unsigned int nBlkVersion;
    uint1024 hashPrevBlock;
    uint512 hashMerkleRoot;
    unsigned int nHeight;
    unsigned int nChannel;
    unsigned int nBits;
    uint64 nNonce;
    
    IMPLEMENT_SERIALIZE
    (
        READWRITE(nBlkVersion);
        //nVersion = this->nVersion;
        READWRITE(hashPrevBlock);
        READWRITE(hashMerkleRoot);
        READWRITE(nHeight);
        READWRITE(nChannel);
        READWRITE(nBits);
        READWRITE(nNonce);
    )
    
    CBlock()
    {
        SetNull();
    }
    
    void SetNull()
    {
        nBlkVersion = 0;
        hashPrevBlock = 0;
        hashMerkleRoot = 0;
        nHeight = 0;
        nChannel = 0;
        nNonce   = 0;
    }
    
    void SetRandom()
    {
        nBlkVersion = GetRandInt(1000);
        nHeight  = GetRandInt(1000);
        nChannel = GetRandInt(1000);
        nBits    = GetRandInt(1000);
        nNonce   = GetRand(1000000);
        
        hashMerkleRoot = GetRand512();
        hashPrevBlock  = GetRand1024();
    }
    
    uint1024 GetHash() const
    {
        return LLC::HASH::SK1024(BEGIN(nBlkVersion), END(nNonce));
    }
    
    void Print()
    {
        printf("CBlock(nVersion=%u, hashPrevBlock=%s, hashMerkleRoot=%s, nHeight=%u, nChannel=%u, nBits=%u, nNonce=%" PRIu64 ", hash=" ANSI_COLOR_BRIGHT_BLUE "%s" ANSI_COLOR_RESET ")\n", nBlkVersion, hashPrevBlock.ToString().c_str(), hashMerkleRoot.ToString().c_str(), nHeight, nChannel, nBits, nNonce, GetHash().ToString().c_str());
    }
};


class TestDB : public LLD::SectorDatabase
{
public:
    TestDB(const char* pszMode="r+") : SectorDatabase("testdb", "testdb", pszMode) {}
    
    bool WriteBlock(uint1024 hash, CBlock blk)
    {
        return Write(hash, blk);
    }
    
    bool ReadBlock(uint1024 hash, CBlock& blk)
    {
        return Read(hash, blk);
    }
};


int main(int argc, char** argv)
{
    ParseParameters(argc, argv);
    
    std::vector<unsigned char> vKey = {5, 6, 10, 11};
    
    Timer timer1;
    timer1.Start();
    unsigned int hash = LLC::HASH::SK64(vKey);
    hash = hash % 65536;
    unsigned int nTime = timer1.ElapsedMicroseconds();
    printf("LLC::HASH::SK64 %u\n", nTime);
    
    timer1.Reset();
    hash = ((vKey[0] << 8) + vKey[1]);
    nTime = timer1.ElapsedMicroseconds();
    printf("Bitwise Shifts %u\n", nTime);
    
    printf("Lower Level Library Initialization...\n");
    
    TestDB* db = new TestDB();
    
    CBlock test;
    test.SetRandom();
    
    std::map<uint1024, CBlock> mapBlocks;
    std::vector<uint1024> vBlocks;
    CBlock bulkTest;
    bulkTest.SetRandom();
    
    unsigned int nTotalRecords = GetArg("-testwrite", 1);
    for(int i = 0; i < nTotalRecords; i++)
    {
        bulkTest.nChannel = i;
        mapBlocks[bulkTest.GetHash()] = bulkTest;
        vBlocks.push_back(bulkTest.GetHash());
    }
    
    printf(ANSI_COLOR_BRIGHT_BLUE "\nWriting %u Keys to Nexus LLD, Ripple NuDB, Google LevelDB, and Oracle BerkeleyDB\n\n" ANSI_COLOR_RESET, nTotalRecords);
    
    unsigned int nTotalElapsed = 0;
    Timer timer;
    timer.Start();
    for(typename std::map< uint1024, CBlock >::iterator blk = mapBlocks.begin(); blk != mapBlocks.end(); blk++ )
    {
        db->WriteBlock(blk->first, blk->second);
        blk->second.Print();
    }
    
    unsigned int nElapsed = timer.ElapsedMicroseconds();
    printf(ANSI_COLOR_GREEN "LLD Write Performance: %u micro-seconds | %f ops/s\n" ANSI_COLOR_RESET, nElapsed, (nTotalRecords * 1000000.0) / nElapsed);
    nTotalElapsed += nElapsed;
    

    
    timer.Reset();
    std::random_shuffle(vBlocks.begin(), vBlocks.end());
    for(auto hash : vBlocks)
    {
        db->ReadBlock(hash, test);
    }
    
    nElapsed = timer.ElapsedMicroseconds();
    printf(ANSI_COLOR_GREEN "LLD Read Performance: %u micro-seconds | %f ops/s\n" ANSI_COLOR_RESET, nElapsed, (nTotalRecords * 1000000.0) / nElapsed);
    nTotalElapsed += nElapsed;
    
    timer.Reset();
    delete db;
    nElapsed = timer.ElapsedMicroseconds();
    printf(ANSI_COLOR_GREEN "LLD Destruct Performance: %u micro-seconds\n" ANSI_COLOR_RESET, nElapsed);
    nTotalElapsed += nElapsed;
    
    printf(ANSI_COLOR_YELLOW "LLD Total Running Time: %f seconds | %f ops/s\n\n" ANSI_COLOR_RESET, nTotalElapsed / 1000000.0, (nTotalRecords * 1000000.0) / nTotalElapsed);
    nTotalElapsed = 0;
    
    
    // Set up database connection information and open database
    leveldb::DB* ldb;
    leveldb::Options options;
    options.create_if_missing = true;
    //options.write_buffer_size = 32 * 1024 * 1024;
    //options.block_size = 256000;

    leveldb::Status status = leveldb::DB::Open(options, GetDataDir().string() + "/leveldb" , &ldb);

    if (false == status.ok())
    {
        //return error("Unable to Open Leveldb\n");
    }
    
    // Add 256 values to the database
    leveldb::WriteOptions writeOptions;
    leveldb::ReadOptions readoptions;
    
    timer.Reset();
    for(typename std::map< uint1024, CBlock >::iterator blk = mapBlocks.begin(); blk != mapBlocks.end(); blk++ )
    {
        CDataStream ssKey(SER_LLD, DATABASE_VERSION);
        ssKey << blk->first;
        
        std::vector<char> vKey(ssKey.begin(), ssKey.end());
        
        CDataStream ssData(SER_LLD, DATABASE_VERSION);
        ssData << blk->second;
        
        std::vector<char> vData(ssData.begin(), ssData.end());
        
        leveldb::Slice slKey(&vKey[0], vKey.size());
        leveldb::Slice slData(&vData[0], vData.size());
        
        ldb->Put(writeOptions, slKey, slData);
    }
    nElapsed = timer.ElapsedMicroseconds();
    printf(ANSI_COLOR_GREEN "LevelDB Write Performance: %u micro-seconds | %f ops/s\n" ANSI_COLOR_RESET, nElapsed, (nTotalRecords * 1000000.0) / nElapsed);
    nTotalElapsed += nElapsed;
    
    
    timer.Reset();
    std::random_shuffle(vBlocks.begin(), vBlocks.end());
    for(auto hash : vBlocks)
    {
        CDataStream ssKey(SER_LLD, DATABASE_VERSION);
        ssKey << hash;
        
        std::vector<char> vKey(ssKey.begin(), ssKey.end());

        leveldb::Slice slKey(&vKey[0], vKey.size());
        std::string strValue;
        
        ldb->Get(readoptions, slKey, &strValue);
    }
    nElapsed = timer.ElapsedMicroseconds();
    printf(ANSI_COLOR_GREEN "LevelDB Read Performance: %u micro-seconds | %f ops/s\n" ANSI_COLOR_RESET, nElapsed, (nTotalRecords * 1000000.0) / nElapsed);
    nTotalElapsed += nElapsed;
    
    timer.Reset();
    delete ldb;
    nElapsed = timer.ElapsedMicroseconds();
    printf(ANSI_COLOR_GREEN "LevelDB Destruct Performance: %u micro-seconds\n" ANSI_COLOR_RESET, nElapsed);
    nTotalElapsed += nElapsed;
    
    
    printf(ANSI_COLOR_YELLOW "LevelDB Total Running Time: %f seconds | %f ops/s\n\n" ANSI_COLOR_RESET, nTotalElapsed / 1000000.0, (nTotalRecords * 1000000.0) / nTotalElapsed);
    nTotalElapsed = 0;
    
    
    return 0;
}
