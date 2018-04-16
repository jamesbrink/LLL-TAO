# Building.

```shell
mkdir build
cd build
cmake ..
make
```

### Running the benchmark

From the build directory.

```shell
./ldd_benchmark -testwrite=100000

Lower Level Library Initialization...
[DATABASE] Checking File /Users/jbrink/Library/Application Support/LLL/keychain/testdb-0.keys
[DATABASE] Keychain File 0 Loading [1717144 bytes]...
[DATABASE] Checking File /Users/jbrink/Library/Application Support/LLL/keychain/testdb-1.keys
[DATABASE] Keychain Initialized with 12008 Keys | Total Size 1717144 | Total Files 1 | Current Size 1717144

Writing 100000 Keys to Nexus LLD, Google LevelDB, and Oracle BerkeleyDB

LLD Write Performance: 3628926 micro-seconds | 27556.362406 ops/s
LLD Read Performance: 3762963 micro-seconds | 26574.802888 ops/s
LLD Destruct Performance: 6918995 micro-seconds
LLD Total Running Time: 14.310884 seconds | 6987.688531 ops/s

LevelDB Write Performance: 2700816 micro-seconds | 37025.847003 ops/s
LevelDB Read Performance: 5845320 micro-seconds | 17107.703257 ops/s
LevelDB Destruct Performance: 1848 micro-seconds
LevelDB Total Running Time: 8.547984 seconds | 11698.664855 ops/s

BerkeleyDB Write Performance: 1856509 micro-seconds | 53864.538227 ops/s
BerkeleyDB Read Performance: 1288131 micro-seconds | 77631.855766 ops/s
BerkeleyDB Destruct Performance: 178382 micro-seconds
BerkeleyDB Total Running Time: 3.323022 seconds | 30093.089964 ops/s
```

# Lower Level Library

Series of Templates for developing Crypto, Database, or Protocol. Base templates for the TAO framework, which will inhereit these templates and create higher level functionality such as Tritium, Amine, and Obsidian for Nexus. This framework is the foundation to Nexus and a 3DC. 


### Lower Level Crypto

Set of Operations for handling Crypto including:

* Digital Signatures (ECDSA, Schnorr, Hash Based)
* Hashing (SHA3 / Notable Secure Algorithms)
* Encyrption (Symmetric / Assymetric)
* Post-Quantum Cyrptography (Experimental)


### Lower Level Database

Set of Templates for designing high efficiency database systems. Core templates can be expanded into higher level database types.

* Key Database
* Sector Database
* Transaction Database (ACID)
* Checkpoint Database (Journal)

### Lower Level Protocol

Set of Client / Server templates for efficient data handling. Processes over 10k connections per server. Inhereit and create custom packet types to write a new protocol with ease and no network programming required.

* Data Server
* Listening Server
* Connection Types
* Packet Styles
* Event Triggers
* DDOS Throttling


### Utilities

Set of useful tools for developing any program such as:

* Serialization
* Runtime
* Debug
* Arguments
* Containers
* Configuration
* Sorting
* Allocators


## License

Nexus is released under the terms of the MIT license. See [COPYING](COPYING.MD) for more
information or see https://opensource.org/licenses/MIT.


## Contributing
If you would like to contribute as always submit a pull request. This library development is expected to be on-going, with new higher level templates created for any types of use in the web.


## Applications
This is a foundational piece of Nexus that can be rebased over a Nexus branch to incorporate new LLL features into Nexus. It could also be useful for:

* Custom MySQL Servers
* Custom WebServers
* Custom Protocols
* Scaleable Databases

These core templates can be expanded in any way by inheriting the base templates and creating any type of new backend that one would like.


## Why?
Because a lot of software that we use today for databases, or protocols, or cyrptography was created back in the 1990's as open source software. Since then the industry has expanded and bloated this code causing performance degragation. The aim of these templates is performance in simplicity. Include only what is needed, no more, and no less. This allows extremely high performance and scaleability necessary for the new distributed systems that will continue to evolve over the next few decades.
