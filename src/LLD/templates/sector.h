/*__________________________________________________________________________________________

            (c) Hash(BEGIN(Satoshi[2010]), END(Sunny[2012])) == Videlicet[2017] ++
            
            (c) Copyright The Nexus Developers 2014 - 2017
            
            Distributed under the MIT software license, see the accompanying
            file COPYING or http://www.opensource.org/licenses/mit-license.php.
            
            "fides in stellis, virtus in numeris" - Faith in the Stars, Power in Numbers

____________________________________________________________________________________________*/

#ifndef NEXUS_LLD_TEMPLATES_SECTOR_H
#define NEXUS_LLD_TEMPLATES_SECTOR_H

#include "pool.h"
#include "keychain.h"
#include "transaction.h"

#include "../../Util/include/runtime.h"

#include <unordered_map>

namespace LLD
{
    
    /* Maximum size a file can be in the keychain. */
    const unsigned int MAX_SECTOR_FILE_SIZE = 1024 * 1024 * 1024; //1 GB per File
    
    
    /* Maximum cache buckets for sectors. */
    const unsigned int MAX_SECTOR_CACHE_SIZE = 1024 * 1024; //1 MB Max Cache
    

    /** Base Template Class for a Sector Database. 
        Processes main Lower Level Disk Communications.
        A Sector Database Is a Fixed Width Data Storage Medium.
        
        It is ideal for data structures to be stored that do not
        change in their size. This allows the greatest efficiency
        in fixed data storage (structs, class, etc.).
        
        It is not ideal for data structures that may vary in size
        over their lifetimes. The Dynamic Database will allow that.
        
        Key Type can be of any type. Data lengths are attributed to
        each key type. Keys are assigned sectors and stored in the
        key storage file. Sector files are broken into maximum of 1 GB
        for stability on all systems, key files are determined the same.
        
        Multiple Keys can point back to the same sector to allow multiple
        access levels of the sector. This specific class handles the lower
        level disk communications for the sector database.
        
        If each sector was allowed to be varying sizes it would remove the
        ability to use free space that becomes available upon an erase of a
        record. Use this Database purely for fixed size structures. Overflow
        attempts will trigger an error code.
        
        TODO:: Add in the Database File Searching from Sector Keys. Allow Multiple Files.
        
    **/
    class SectorDatabase
    {
    protected:
        /* Mutex for Thread Synchronization. 
            TODO: Lock Mutex based on Read / Writes on a per Sector Basis. 
            Will allow higher efficiency for thread concurrency. */
        Mutex_t SECTOR_MUTEX;
        
        
        /* The String to hold the Disk Location of Database File. */
        std::string strLocation;
        
        
        /* The String to hold the Disk Location of the Journal. */
        std::string strJournal;
        
        
        /* The nameof the Keychain Registry. */
        std::string strKeychainRegistry;
        
        
        /* Read only Flag for Sectors. */
        bool fReadOnly = false;
        
        
        /* Destructor Flag. */
        bool fDestruct = false;
        
        
        /* Initialize Flag. */
        bool fInitialized = false;
        
        
        /* Timer for Runtime Calculations. */
        Timer runtime;
        
        
        /* Transaction Data Flags. */
        bool fTransaciton = false;
        
        
        /* Sector Keys Database. */
        KeyDatabase* SectorKeys;
        
        
        /* Hashmap Custom Hash Using SK. */
        struct SK_Hashmap
        {
            std::size_t operator()(const std::vector<unsigned char>& k) const {
                return LLC::HASH::SK32(k);
            }
        };
        
        //std::unordered_map< std::vector<unsigned char>, std::vector<unsigned char>, SK_Hashmap > mapRecordCache;
        //std::map< std::vector<unsigned char>, std::vector<unsigned char> > mapRecordCache[MAX_SECTOR_CACHE_BUCKETS];
        CachePool* CachePool;
        
        /* The current File Position. */
        mutable unsigned int nCurrentFile;
        mutable unsigned int nCurrentFileSize;
        
        /* Cache Writer Thread. */
        Thread_t CacheWriterThread;
        
    public:
        /** The Database Constructor. To determine file location and the Bytes per Record. **/
        SectorDatabase(std::string strName, std::string strKeychain, const char* pszMode="r+") : CachePool(new CachePool(MAX_SECTOR_CACHE_SIZE)), CacheWriterThread(boost::bind(&SectorDatabase::CacheWriter, this))
        {
            /* Create the Sector Database Directories. */
            boost::filesystem::path dir(GetDataDir().string() + "/datachain");
            if(!boost::filesystem::exists(dir))
                boost::filesystem::create_directory(dir);
            
            
            /* Create the Sector Database Directories. */
            boost::filesystem::path dir(GetDataDir().string() + "/journal");
            if(!boost::filesystem::exists(dir))
                boost::filesystem::create_directory(dir);
            
            strKeychainRegistry = strKeychain;
            strLocation = GetDataDir().string() + "/datachain/" + strName;
            strJournal  = GetDataDir().string() + "/journal/" + strName;
            
            nCurrentFile = 0;
            
            /** Read only flag when instantiating new database. **/
            fReadOnly = (!strchr(pszMode, '+') && !strchr(pszMode, 'w'));
            
            Initialize();
        }
        
        ~SectorDatabase()
        {
            fDestruct = true;
            
            CacheWriterThread.join();
            
            delete CachePool;
        }
        
        
        /** Initialize Sector Database. **/
        void Initialize()
        {
            if(GetBoolArg("-runtime", false))
                runtime.Start();
            
            /* Try to recover if a txlog fails */
            std::string strFilename = strprintf("%s-txlog.dat", strJournal.c_str());
            std::fstream fileIncoming(strFilename, std::ios::in | std::ios::binary);
            if(fileIncoming)
            {
                
                fileJournal.ignore(std::numeric_limits<std::streamsize>:max());
                unsigned int nSize = fileJournal.gcount();
                
                fileJournal.seekg (0, std::ios::beg);
                std::vector<unsigned char> vJournal(nSize, 0);
                fileJournal.read((char*) &vJournal[0], vJournal.size());
                fileJournal.close();
                
                /* Iterate the Data of Transaction Log. */
                unsigned int nIterator = 0;
                while(nIterator < nSize)
                {
                    unsigned short nKeySize = (vJournal[nIterator] << 8) + vJournal[nIterator + 1];
                    std::vector<unsigned char> vKey(vJournal.begin() + nIterator + 2, vJournal.begin() + nIterator + 2 + nKeySize);
                    
                    nIterator += nKeySize + 2;
                    unsigned short nDataSize = (vJournal[nIterator] << 8) + vJournal[nIterator + 1];
                    std::vector<unsigned char> vData(vJournal.begin() + nIterator + 2, vJournal.begin() + nIterator + 2 + nDataSize);
                    
                    nIterator += nDataSize + 2;
                    
                    /* Recover From Journal Data. */
                    Put(vKey, vData);
                }
                
                
                /* Delete the txlog file. */
                std::remove(strJournal.c_str());
            }
            
            
            /* Find the most recent append file. */
            while(true)
            {
            
                /* TODO: Make a worker or thread to check sizes of files and automatically create new file.
                    Keep independent of reads and writes for efficiency. */
                std::string strFilename = strprintf("%s-%u.dat", strLocation.c_str(), nCurrentFile);
                std::fstream fileIncoming(strFilename, std::ios::in | std::ios::binary);
                if(!fileIncoming) {
                    
                    /* Assign the Current Size and File. */
                    if(nCurrentFile > 0)
                        nCurrentFile--;
                    else
                    {
                        /* Create a new file if it doesn't exist. */
                        std::ofstream cStream(strprintf("%s-%u.dat", strLocation.c_str(), nCurrentFile).c_str(), std::ios::binary);
                        cStream.close();
                    }
                    
                    break;
                }
                
                /* Get the Binary Size. */
                fileIncoming.ignore(std::numeric_limits<std::streamsize>::max());
                nCurrentFileSize = fileIncoming.gcount();
                fileIncoming.close();
                
                /* Increment the Current File */
                nCurrentFile++;
            }
            
            /* Register the Keychain in Global LLD scope if it hasn't been registered already. */
            if(!KeychainRegistered(strKeychainRegistry))
                RegisterKeychain(strKeychainRegistry);
            
                        
            /** Read a Record from Binary Data. **/
            SectorKeys = GetKeychain(strKeychainRegistry);
            if(!SectorKeys)
                error("LLD::Sector::Initialized() : Sector Keys not Registered for Name %s\n", strKeychainRegistry.c_str());
            
            if(GetBoolArg("-runtime", false))
                printf(ANSI_COLOR_GREEN "LLD::Sector::Initialized() executed in %u micro-seconds\n" ANSI_COLOR_RESET, runtime.ElapsedMicroseconds());
            
            fInitialized = true;
        }
        
        
        
        /* Get the keys for this sector database from the keychain.  */
        std::vector< std::vector<unsigned char> > GetKeys() { return SectorKeys->GetKeys(); }
        
        
        template<typename Key>
        bool Exists(const Key& key)
        {
            /** Serialize Key into Bytes. **/
            CDataStream ssKey(SER_LLD, DATABASE_VERSION);
            ssKey.reserve(GetSerializeSize(key, SER_LLD, DATABASE_VERSION));
            ssKey << key;
            std::vector<unsigned char> vKey(ssKey.begin(), ssKey.end());
            
            /** Return the Key existance in the Keychain Database. **/
            unsigned int nBucket = 0, nIterator = 0;
            return SectorKeys->Find(vKey, nBucket, nIterator);
        }
        
        
        template<typename Key>
        bool Erase(const Key& key)
        {
            if(GetBoolArg("-runtime", false))
                runtime.Start();
            
            /** Serialize Key into Bytes. **/
            CDataStream ssKey(SER_LLD, DATABASE_VERSION);
            ssKey.reserve(GetSerializeSize(key, SER_LLD, DATABASE_VERSION));
            ssKey << key;
            std::vector<unsigned char> vKey(ssKey.begin(), ssKey.end());
            
            if(GetBoolArg("-runtime", false))
                printf(ANSI_COLOR_GREEN "LLD::Sector::Erase() executed in %u micro-seconds\n" ANSI_COLOR_RESET, runtime.ElapsedMicroseconds());
            
            /* Return the Key existance in the Keychain Database. */
            CachePool->Remove(vKey); //TODO: Transaction Erase
            
            return SectorKeys->Erase(vKey);
        }
        
        
        template<typename Key, typename Type>
        bool Read(const Key& key, Type& value)
        {
            /** Serialize Key into Bytes. **/
            CDataStream ssKey(SER_LLD, DATABASE_VERSION);
            ssKey << key;
            std::vector<unsigned char> vKey(ssKey.begin(), ssKey.end());
            
            /** Get the Data from Sector Database. **/
            std::vector<unsigned char> vData;
            if(!Get(vKey, vData))
                return false;

            /** Deserialize Value. **/
            try {
                CDataStream ssValue(vData, SER_LLD, DATABASE_VERSION);
                ssValue >> value;
            }
            catch (std::exception &e) {
                return false;
            }


            return true;
        }
        

        template<typename Key, typename Type>
        bool Write(const Key& key, const Type& value)
        {
            if (fReadOnly)
                assert(!"Write called on database in read-only mode");

            /** Serialize the Key. **/
            CDataStream ssKey(SER_LLD, DATABASE_VERSION);
            ssKey << key;
            std::vector<unsigned char> vKey(ssKey.begin(), ssKey.end());

            /** Serialize the Value **/
            CDataStream ssValue(SER_LLD, DATABASE_VERSION);
            ssValue << value;
            std::vector<unsigned char> vData(ssValue.begin(), ssValue.end());
            
            return Put(vKey, vData);
        }
        
        /** Get a Record from the Database with Given Key. **/
        bool Get(std::vector<unsigned char> vKey, std::vector<unsigned char>& vData)
        {
            LOCK(SECTOR_MUTEX);
            
            if(CachePool->Get(vKey, vData))
                return true;
            
            unsigned int nBucket = 0, nIterator = 0;
            if(SectorKeys->Find(vKey, nBucket, nIterator))
            {
                
                /* Read the Sector Key from Keychain. */
                SectorKey cKey;
                if(!SectorKeys->Get(vKey, cKey, nBucket, nIterator))
                    return false;
                

                /* Open the Stream to Read the data from Sector on File. */
                std::string strFilename = strprintf("%s-%u.dat", strLocation.c_str(), cKey.nSectorFile);
                std::fstream fStream(strFilename.c_str(), std::ios::in | std::ios::out | std::ios::binary);
                if(!fStream)
                    return error("Sector File %s Doesn't Exist\n", strFilename.c_str());
                
                /* Seek to the Sector Position on Disk. */
                fStream.seekg(cKey.nSectorStart, std::ios::beg);
                
                //TODO: Add Sector Data available checks. WILL CHECK IF DATABASE FAILED TO FINISH WRITING SECTOR
            
                /* Read the State and Size of Sector Header. */
                vData.resize(cKey.nSectorSize);
                fStream.read((char*) &vData[0], vData.size());
                fStream.close();
                
                /** Check the Data Integrity of the Sector by comparing the Checksums. **/
                unsigned int nChecksum = LLC::HASH::SK32(vData);
                if(cKey.nChecksum != nChecksum)
                    return error("Sector Get() : Checksums don't match data. Corrupted Sector (%u - %u)", nChecksum, cKey.nChecksum);
                
                if(GetArg("-verbose", 0) >= 4)
                    printf("SECTOR GET:%s\n", HexStr(vData.begin(), vData.end()).c_str());
                
                return true;
            }
            else
                return error("SECTOR GET::KEY NOT FOUND");
            
            return false;
        }
        
        
        /* Add / Update A Record in the Database */
        bool Put(std::vector<unsigned char> vKey, std::vector<unsigned char> vData)
        {
            LOCK(SECTOR_MUTEX);
            
            /* Only use Cache Pool when Initialized. */
            if(fInitialized)
            {
                
                /* Write to Cache Pool Memory First if Possible. */
                if(!GetBoolArg("-forcewrite", false))
                {
                    CachePool->Put(vKey, vData, fTransaction ? PENDING_TX : PENDING_WRITE);
                        
                    return true;
                }
                
                /* Write Transactions to Cache Pool. */
                else if(fTransaction)
                {
                    CachePool->Put(vKey, vData, PENDING_TX);
                    
                    return true;
                }
                else
                    CachePool->Put(vKey, vData, MEMORY_ONLY);
            
            }
            
            /* Runtime Calculations. */
            if(GetBoolArg("-runtime", false))
                runtime.Start();
            
            /* Only if the Writes are forced. */
            unsigned int nIterator = 0, nBucket = 0;
            if(!SectorKeys->Find(vKey, nBucket, nIterator))
            {
                if(nCurrentFileSize > MAX_SECTOR_FILE_SIZE)
                {
                    if(GetArg("-verbose", 0) >= 4)
                        printf("SECTOR::Put(): Current File too Large, allocating new File %u\n", nCurrentFileSize, nCurrentFile + 1);
                        
                    nCurrentFile ++;
                    nCurrentFileSize = 0;
                    
                    std::ofstream fStream(strprintf("%s-%u.dat", strLocation.c_str(), nCurrentFile).c_str(), std::ios::out | std::ios::binary);
                    fStream.close();
                }
                
                /* Open the Stream to Read the data from Sector on File. */
                std::string strFilename = strprintf("%s-%u.dat", strLocation.c_str(), nCurrentFile);
                std::fstream fStream(strFilename.c_str(), std::ios::in | std::ios::out | std::ios::binary);
                
                /* If it is a New Sector, Assign a Binary Position. 
                    TODO: Track Sector Database File Sizes. */
                fStream.seekp(nCurrentFileSize, std::ios::beg);
                fStream.write((char*) &vData[0], vData.size());
                fStream.close();
                
                /* Create a new Sector Key. */
                SectorKey cKey(READY, vKey, nCurrentFile, nCurrentFileSize, vData.size()); 
                
                /* Check the Data Integrity of the Sector by comparing the Checksums. */
                cKey.nChecksum    = LLC::HASH::SK32(vData);
                
                /* Increment the current filesize */
                nCurrentFileSize += vData.size();
                
                /* Assign the Key to Keychain. */
                SectorKeys->Put(cKey, nBucket, nIterator);
                
                /* Update the Data in the Cache Pool. */
                CachePool->SetState(key.vKey, MEMORY_ONLY);
            }
            else
            {
                /* Get the Sector Key from the Keychain. */
                SectorKey cKey;
                if(!SectorKeys->Get(vKey, cKey, nBucket, nIterator))
                    return false;
                    
                /* Open the Stream to Read the data from Sector on File. */
                std::string strFilename = strprintf("%s-%u.dat", strLocation.c_str(), cKey.nSectorFile);
                std::fstream fStream(strFilename.c_str(), std::ios::in | std::ios::out | std::ios::binary);
                
                /* Locate the Sector Data from Sector Key. 
                    TODO: Make Paging more Efficient in Keys by breaking data into different locations in Database. */
                fStream.seekp(cKey.nSectorStart, std::ios::beg);
                if(vData.size() > cKey.nSectorSize){
                    fStream.close();
                    printf("ERROR PUT (TOO LARGE) NO TRUNCATING ALLOWED (Old %u :: New %u):%s\n", cKey.nSectorSize, vData.size(), HexStr(vData.begin(), vData.end()).c_str());
                    
                    return false;
                }
                
                /* Assign the Writing State for Sector. */
                //TODO: use memory maps
                fStream.write((char*) &vData[0], vData.size());
                fStream.close();
                
                /* Update the Keychain. */
                cKey.nState    = READY;
                cKey.nChecksum = LLC::HASH::SK32(vData);
                SectorKeys->Put(cKey, nBucket, nIterator);
                
                /* Update the Data in the Cache Pool. */
                CachePool->SetState(key.vKey, MEMORY_ONLY);
            }
            
            if(GetArg("-verbose", 0) >= 4)
                printf("SECTOR PUT:%s\nCurrent File: %u | Current File Size: %u\n", HexStr(vData.begin(), vData.end()).c_str(), nCurrentFile, nCurrentFileSize);
        
            if(GetBoolArg("-runtime", false))
                printf(ANSI_COLOR_GREEN "LLD::Sector::Write() executed in %u micro-seconds\n" ANSI_COLOR_RESET, runtime.ElapsedMicroseconds());
            
            return true;
        }
        
        
        /* Helper Thread to Batch Write to Disk. */
        void CacheWriter()
        {
            
            //TODO: Add this to data journal with data to be committed first. 
            //This will tell the database on next keychain init if there was any failed writes for graceful recovery
            while(true)
            {
                /* Wait for Database to Initialize. */
                if(!fInitialized)
                {
                    Sleep(1);
                    
                    continue;
                }
                
                /* Check for data to be written. */
                std::vector< std::pair<std::vector<unsigned char>, std::vector<unsigned char>> > vIndexes;
                if(!CachePool->GetDiskBuffer(vIndexes))
                {
                    if(fDestruct)
                        return;
                    
                    Sleep(10, true);
                    
                    continue;
                }
                
                /* Allocate new File if Needed. */
                if(nCurrentFileSize > MAX_SECTOR_FILE_SIZE)
                {
                    if(GetArg("-verbose", 0) >= 4)
                        printf("SECTOR::Put(): Current File too Large, allocating new File %u\n", nCurrentFileSize, nCurrentFile + 1);
                                
                    nCurrentFile ++;
                    nCurrentFileSize = 0;
                            
                    std::ofstream fStream(strprintf("%s-%u.dat", strLocation.c_str(), nCurrentFile).c_str(), std::ios::out | std::ios::binary);
                    fStream.close();
                }
                
                /* Temp Variable for Reads / Writes. */
                unsigned int nTempFileSize = nCurrentFileSize;
                
                /* Go through and do overwrite operations. */
                std::vector< unsigned char > vBatch;
                std::vector< SectorKey > vBatchKeys;
                for(auto vObj : vIndexes)
                {
                    LOCK(SECTOR_MUTEX);
                    
                    /* Setup for batch write on first update. */
                    unsigned int nIterator = 0, nBucket = 0;
                    if(!SectorKeys->Find(vObj.first, nBucket, nIterator))
                    {
                        /* Create a new Sector Key. */
                        SectorKey cKey(READY, vObj.first, nCurrentFile, nTempFileSize, vObj.second.size(), nBucket, nIterator); 
                        
                        /* Check the Data Integrity of the Sector by comparing the Checksums. */
                        cKey.nChecksum    = LLC::HASH::SK32(vObj.second);
                        
                        /* Increment the current filesize */
                        nTempFileSize += vObj.second.size();
                        
                        /* Setup the Batch data write. */
                        vBatch.insert(vBatch.end(), vObj.second.begin(), vObj.second.end());
                        vBatchKeys.push_back(cKey);
                    }
                    else
                    {
                        /* Get the Sector Key from the Keychain. */
                        SectorKey cKey;
                        if(!SectorKeys->Get(vObj.first, cKey, nBucket, nIterator))
                            break;
                            
                        /* Open the Stream to Read the data from Sector on File. */
                        std::string strFilename = strprintf("%s-%u.dat", strLocation.c_str(), cKey.nSectorFile);
                        std::fstream fStream(strFilename.c_str(), std::ios::in | std::ios::out | std::ios::binary);
                        
                        /* Locate the Sector Data from Sector Key. 
                            TODO: Make Paging more Efficient in Keys by breaking data into different locations in Database. */
                        fStream.seekp(cKey.nSectorStart, std::ios::beg);
                        if(vObj.second.size() > cKey.nSectorSize){
                            fStream.close();
                            printf("ERROR PUT (TOO LARGE) NO TRUNCATING ALLOWED (Old %u :: New %u):%s\n", cKey.nSectorSize, vObj.second.size(), HexStr(vObj.second.begin(), vObj.second.end()).c_str());
                            
                            break;
                        }
                        
                        /* Write the new data to the sector. */
                        fStream.write((char*) &vObj.second[0], vObj.second.size());
                        fStream.close();
                        
                        /* Update the Keychain. */
                        cKey.nState    = READY;
                        cKey.nChecksum = LLC::HASH::SK32(vObj.second);
                        
                        /* Force Write per Key. */
                        SectorKeys->Put(cKey, nBucket, nIterator);
                    }
                }
                
                /* Write the data in one operation. */
                if(vBatch.size() > 100 || fDestruct)
                {
                    LOCK(SECTOR_MUTEX);
                    
                    /* Open the Stream to Read the data from Sector on File. */
                    std::string strFilename = strprintf("%s-%u.dat", strLocation.c_str(), nCurrentFile);
                    std::fstream fStream(strFilename.c_str(), std::ios::in | std::ios::out | std::ios::binary);
                    
                    /* If it is a New Sector, Assign a Binary Position. 
                        TODO: Track Sector Database File Sizes. */
                    fStream.seekp(nCurrentFileSize, std::ios::beg);
                    fStream.write((char*) &vBatch[0], vBatch.size());
                    fStream.close();
                    
                    /* Set the new current file size. */
                    nCurrentFileSize = nTempFileSize;
                    
                    /* Write all the keys after disk operations. */
                    for(auto key : vBatchKeys)
                    {
                        SectorKeys->Put(key, key.nBucket, key.nIterator);
                        CachePool->SetState(key.vKey, MEMORY_ONLY);
                    }
                }
            }
        }
        
        /** Start a New Database Transaction. 
            This will put all the database changes into pending state.
            If any of the database updates fail in procewss it will roll the database back to its previous state. **/
        void TxnBegin()
        {
            if(fTransaction)
                Remove(PENDING_TX);
            else
                fTransaction = true;
        }
        
        /** Abort the current transaction that is pending in the transaction chain. **/
        void TxnAbort()
        {
            /** Delete the previous transaction pointer if applicable. **/
            if(fTransaction)
                Remove(PENDING_TX);
        }
        
        
        bool TxnRollback()
        {
            return false;
        }
        
        /** Commit the Data in the Transaction Object to the Database Disk. */
        bool TxnCommit()
        {
            /* Check for data to be written. */
            std::vector< std::pair<std::vector<unsigned char>, std::vector<unsigned char>> > vIndexes;
            if(!CachePool->GetTransactionBuffer(vIndexes))
                return false;
            
            /* Create the Journal Buffer. */
            std::vector<unsigned char> vJournal;
            for(auto vObj : vIndexes)
            {
                unsigned short nKeySize = vObj.first.size();
                vJournal.push_back(nKeySize >> 8);
                vJournal.push_back(nKeySize);
                vJounral.insert(vJournal.end(), vObj.first.begin(), vObj.first.end());
                    
                unsigned short nDataSize = vObj.second.size();
                vJournal.push_back(nDataSize >> 8);
                vJournal.push_back(nDataSize);
                vJournal.insert(vJournal.end(), vObj.second.begin(), vOBj.second.end());
            }
            
            /* Create txlog for Recovery. */
            std::string strFilename = strprintf("%s-txlog.dat", strJournal.c_str());
            std::fstream fileJournal(strFilename, std::ios::out | std::ios::trunc | std::ios::binary);
            fileJournal.write((char*) &vJournal[0], vJournal.size());
            fileJournal.close();
                
            /* Allocate new File if Needed. */
            if(nCurrentFileSize > MAX_SECTOR_FILE_SIZE)
            {
                if(GetArg("-verbose", 0) >= 4)
                    printf("SECTOR::Put(): Current File too Large, allocating new File %u\n", nCurrentFileSize, nCurrentFile + 1);
                                
                nCurrentFile ++;
                nCurrentFileSize = 0;
                            
                std::ofstream fStream(strprintf("%s-%u.dat", strLocation.c_str(), nCurrentFile).c_str(), std::ios::out | std::ios::binary);
                fStream.close();
            }
                
            /* Temp Variable for Reads / Writes. */
            unsigned int nTempFileSize = nCurrentFileSize;
                
            /* Go through and do overwrite operations. */
            std::vector< unsigned char > vBatch;
            std::vector< SectorKey > vBatchKeys;
            for(auto vObj : vIndexes)
            {
                LOCK(SECTOR_MUTEX);
                    
                /* Setup for batch write on first update. */
                unsigned int nIterator = 0, nBucket = 0;
                if(!SectorKeys->Find(vObj.first, nBucket, nIterator))
                {
                    /* Create a new Sector Key. */
                    SectorKey cKey(READY, vObj.first, nCurrentFile, nTempFileSize, vObj.second.size()); 
                        
                    /* Check the Data Integrity of the Sector by comparing the Checksums. */
                    cKey.nChecksum    = LLC::HASH::SK32(vObj.second);
                        
                    /* Increment the current filesize */
                    nTempFileSize += vObj.second.size();
                        
                    /* Assign the Key to Keychain. */
                    SectorKeys->Put(cKey, nBucket, nIterator);
                        
                    /* Setup the Batch data write. */
                    vBatch.insert(vBatch.end(), vObj.second.begin(), vObj.second.end());
                    vBatchKeys.push_back(cKey);
                }
                else
                {
                    /* Get the Sector Key from the Keychain. */
                    SectorKey cKey;
                    if(!SectorKeys->Get(vObj.first, cKey, nBucket, nIterator))
                        return TxnRollback();
                            
                    /* Open the Stream to Read the data from Sector on File. */
                    std::string strFilename = strprintf("%s-%u.dat", strLocation.c_str(), cKey.nSectorFile);
                    std::fstream fStream(strFilename.c_str(), std::ios::in | std::ios::out | std::ios::binary);
                        
                    /* Locate the Sector Data from Sector Key. 
                        TODO: Make Paging more Efficient in Keys by breaking data into different locations in Database. */
                    fStream.seekp(cKey.nSectorStart, std::ios::beg);
                    if(vObj.second.size() > cKey.nSectorSize){
                        fStream.close();
                        printf("ERROR PUT (TOO LARGE) NO TRUNCATING ALLOWED (Old %u :: New %u):%s\n", cKey.nSectorSize, vObj.second.size(), HexStr(vObj.second.begin(), vObj.second.end()).c_str());
                            
                        return TxnRollback();
                    }
                        
                    /* Write the new data to the sector. */
                    fStream.write((char*) &vObj.second[0], vObj.second.size());
                    fStream.close();
                        
                    /* Update the Keychain. */
                    cKey.nState    = READY;
                    cKey.nChecksum = LLC::HASH::SK32(vObj.second);
                    cKey.nBucket   = nBucket;
                    cKey.nIterator = nIterator;
                    
                    vBatchKeys.push_back(cKey);
                }
            }
                
            /* Write the data in one operation. */
            if(vBatch.size() > 0)
            {
                /* Open the Stream to Read the data from Sector on File. */
                std::string strFilename = strprintf("%s-%u.dat", strLocation.c_str(), nCurrentFile);
                std::fstream fileStream(strFilename.c_str(), std::ios::in | std::ios::out | std::ios::binary);
                    
                /* If it is a New Sector, Assign a Binary Position. 
                    TODO: Track Sector Database File Sizes. */
                fileStream.seekp(nCurrentFileSize, std::ios::beg);
                fileStream.write((char*) &vBatch[0], vBatch.size());
                fileStream.close();
                    
                /* Set the new current file size. */
                nCurrentFileSize = nTempFileSize;
            }
            
            /* Write the Keys to Disk and Update Memory Pool. */
            for(auto key : vBatchKeys)
            {
                SectorKeys->Put(key, key.nBucket, key.nIterator);
                CachePool->SetState(key.vKey, MEMORY_ONLY);
            }
            
            /* Remove the Journal if Successful. */
            std::remove(strFilename);
            
            return true;
        }
        
    };
}

#endif
