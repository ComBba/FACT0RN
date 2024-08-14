#  <div align="center">  FACT0RN  </div>

A blockchain replacing hashing as Proof of Work (PoW) by integer factorization. A fork from bitcoin V22.0. The proof-of-work component has been replaced by Integer Factorization but everything else remains intact including the cli tool and all the RPC calls.

The FACT0RN blockchain seeks to allow its user to pay FACT coins to place integers in a deadpool for factorization. 

Website: https://fact0rn.io <br>
Whitepaper: [https://drive.google.com/file/d/1AJ5_MTIhdI-lz8X35WGi20JNnbN_q2vn/view](https://drive.google.com/file/d/1AJ5_MTIhdI-lz8X35WGi20JNnbN_q2vn/view) <br>
Coinbase: https://blog.coinbase.com/fact0rn-blockchain-integer-factorization-as-proof-of-work-pow-bc48c6f2100b <br>

Contact:

E-mail: fact0rn@pm.me <br>
Discord: [https://discord.gg/tE2BNpgmtH](https://discord.gg/tE2BNpgmtH) <br>
Twitter: [https://twitter.com/FACT0RN](https://twitter.com/FACT0RN) <br>
Reddit: https://www.reddit.com/r/FACT0RN/ <br>

Listed on the following exchanges:
1. https://xeggex.com/asset/fact <br>
2. https://txbit.io/asset/fact <br>
3. https://dex-trade.com/spot/trading/factusdt <br>

## Coin Distribution

The FACT0RN blockchain had no pre-sale, pre-allocation, pre-mining, pre-distribution, or any mechanism to distribute or sell coins in any way prior to launch. The only way to get FACT coins is to mine them yourself. This has been true from the beginning and will continue to be true until FACT0RN is listed on an exchange. (Okay, now that we have been listed you can buy them.)

## Installation

Binaries are provided for Ubuntu 20.04 LTS. To build from source follow these steps:

#### Depends build
From the repository root folder run the following:
```
make -C depends NO_QT=1                                && \
./autogen.sh                                           && \
./configure --prefix=`pwd`/depends/x86_64-pc-linux-gnu && \ 
make
```
    
#### Binaries

You can grab the binaries from the releases page, untar them and run them as-is.

#### Tor

To run nodes using Tor you will need to install Tor. For Ubuntu,

```
sudo apt install tor 
```

Now you will need to add the following three lines to ``/etc/tor/torrc``,

```
ControlPort 9051
CookieAuthentication 1
CookieAuthFileGroupReadable 1
```

Now you can start the factornd client and it will use the Tor Network.

## Running a node

Running a node can be done for two purposes; relaying transactions and/or mining. For relaying transactions
all you need to do is run the executable and you are good to go. If you want to mine then you need to set
a username and password so that your miner is able to connect to it. 

There are two ways to do this; set them in your config file or pass them in using parameters. 

### Method 1: config file

The advantage of this method is that you don't have to enter it anywhere. You can run your node and use
the cli tools without passing in the rpcuser and rpcpassword flags all the time. 

Create the file ``~/.factorn/factorn.conf`` if it does not exist. Add the following to it:

```
rpcuser=<Type a username of your choice>
rpcpassword=<Type in any password of your choosing>
```

That's it. You can start your node or use the cli tools and it will use that username and password automatically.

### Method 2: Command line RPC flags

From the directory where you untar-ed your binaries from the release page do the following:

```
./factornd -rpcuser=<set your username here> -rpcpassword=<set your password here>
```

Your miner will need these to connect to your node and ask for the next block to mine. If you are mining locally
your port are the standard ports: mainnet -> 8332 and testnet -> 18332. By default ``factornd`` runs on mainnet.
To run testnet do:

```
./factornd -rpcuser=<set your username here> -rpcpassword=<set your password here> -testnet
```
You will need to know this to run FACTOR.py from the mining code at ``https://github.com/FACT0RN/factoring``.
 

## Mining

We have created a python script to mine. Here's what you will need to mine:

1. Wallet
2. Python 3
3. Put it all together

First, we need a node running. See the installation section. Once you have your
node running here's how to create a wallet, generate an address and extract the 
scriptPubKey value that will allow you to earn mining rewards.


From the projects root folder:

```
src/factorn-wallet  -wallet=<wallet name>  -descriptors create
src/factorn-cli    loadwallet <wallet name>
src/factorn-cli    getnewaddress
src/factorn-cli    getaddressinfo <address from previous command>
```

For example, from the get ``getaddressinfo`` command above you should get something
similar to:

```
{
  "address": "fact1q4zjycg88kyk72szmhpjvm82mu0zm6p26g7zeh8",
  "scriptPubKey": "0014a8a44c20e7b12de5405bb864cd9d5be3c5bd055a",
  "ismine": true,
  "solvable": true,
  "desc": "wpkh([67f532c6/84'/0'/0'/0/0]02de55517a22dc5a66d4a193cb877a3658b1c456827c1404d18e79dec82d5d937a)#0hcn4tny",
  "parent_desc": "wpkh([67f532c6/84'/0'/0']xpub6D7gQbDdRjuiMN9EqbEzWY4owSUfNNcRdA4E5aaYCX4VoRPMzgeaF4C15D6hSCUpvUkZvjKJTLktDVvjZ3beL8sfW1ATsNQ6qCsAkV6STtr/0/*)#zylutadk",
  "iswatchonly": false,
  "isscript": false,
  "iswitness": true,
  "witness_version": 0,
  "witness_program": "a8a44c20e7b12de5405bb864cd9d5be3c5bd055a",
  "pubkey": "02de55517a22dc5a66d4a193cb877a3658b1c456827c1404d18e79dec82d5d937a",
  "ischange": false,
  "timestamp": 1650416082,
  "hdkeypath": "m/84'/0'/0'/0/0",
  "hdseedid": "0000000000000000000000000000000000000000",
  "hdmasterfingerprint": "67f532c6",
  "labels": [
    ""
  ]
}
```

You will need the value from "scriptPubKey" to mine. In this example, this person
would use the value "0014a8a44c20e7b12de5405bb864cd9d5be3c5bd055a" to pass to the 
python mining script. This would credit the rewards of the block to their address.

Once you have this scriptPubKey value from your wallet clone the miner code from ``https://github.com/FACT0RN/factoring``. 

You will need to install the following python packages:

```
cypari2
gmpy2
numpy
sympy
base58
```

If you are using Anaconda, here are the commands to install the needed packages:

```
conda install -c conda-forge cypari2 
conda install -c anaconda numpy 
conda install -c conda-forge gmpy2 
conda install -c conda-forge sympy 
conda install -c conda-forge base58 
```

You will only need the ``FACTOR.py`` script. 

```
python FACTOR.py <scriptPubKey>
```

<br>
<br>
<br>

# for Windows 10 x64
### 기본 : wsl에 Ubuntu-22.04 설치 및 기본 빌드용 프로그램 설치
```
sudo apt update
sudo apt upgrade
sudo apt install mingw-w64
sudo apt install g++-mingw-w64-x86-64
sudo apt install build-essential libtool autotools-dev automake pkg-config bsdmainutils curl git
sudo apt install nsis
sudo apt-get install unzip
sudo update-alternatives --config x86_64-w64-mingw32-g++
```

### select 1
```
 1            /usr/bin/x86_64-w64-mingw32-g++-posix   30        manual mode
```

### 계속 진행
```
PATH=$(echo "$PATH" | sed -e 's/:\/mnt.*//g') # strip out problematic Windows %PATH% imported var
sudo bash -c "echo 0 > /proc/sys/fs/binfmt_misc/status" # Disable WSL support for Win32 applications.
cd depends
```

### 에러발생시 (No such file or directory)
```
dos2unix *
```

### depends make가 몇십분 걸림
#### ※ 중간에 boost, gmp등 다운로드 Error나 hash에러, curl(22)=404 에러가 발생하여 멈출 경우 다시 명령어 재실행
```
make HOST=x86_64-w64-mingw32
```

### 에러발생시 필요에 따라 별도 실행
```
libtool --finish /mnt/c/Github/FACT0RN/depends/x86_64-w64-mingw32/lib
```

### 다음 에러 등이 발생시 아래 명령어를 사용하고 패치 관련 질문에서 계속 y 입력
patch: **** Can't rename file dbinc/atomic.h.orujksV to dbinc/atomic.h : Permission denied <br>
make: *** [funcs.mk:282: /mnt/c/Github/FACT0RN/depends/work/build/x86_64-w64-mingw32/bdb/4.8.30-a378641a596/.stamp_preprocessed] Error 2
```
chmod -R u+w /mnt/c/Github/FACT0RN/depends/work/build/x86_64-w64-mingw32/
make HOST=x86_64-w64-mingw32
```

### 실제 프로그램 빌드 시작
```
cd ..
find . -name \Makefile|xargs dos2unix
find . -name \*.m4|xargs dos2unix
find . -name \*.ac|xargs dos2unix
find . -name \*.am|xargs dos2unix
find . -name \*.sh|xargs dos2unix
find . -name \*.c|xargs dos2unix
find . -name \*.h|xargs dos2unix
find . -name \*.cpp|xargs dos2unix
find . -name \*.la|xargs dos2unix
./autogen.sh
```
```
CONFIG_SITE=$PWD/depends/x86_64-w64-mingw32/share/config.site ./configure --prefix=/
make
sudo bash -c "echo 1 > /proc/sys/fs/binfmt_misc/status" # Enable WSL support for Win32 applications.
```

### 인스톨 시작
```
mkdir workspace/FACT0RN
make install DESTDIR=/mnt/c/Github/FACT0RN/workspace/FACT0RN
make deploy
```