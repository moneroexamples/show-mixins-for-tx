# Show mixins in each transaction

No tool nor blockchain website exist that shows the mixin outputs used in the inputs of a given transaction.  This is a shame, as mixins represent one of the main
advantages of [Monero](https://getmonero.org/) over other cryptocurrencies.

In this example, this problem is addressed. Specifically, a C++ program called
`showmixins` is developed. The program prints out all mixins used in a given transaction. In addition,
if monero address and privet view key are provided,
the true mixins will be identified.

## Pre-requisites

Everything here was done and tested
on Ubuntu 14.04 x86_64 and Ubuntu 15.10 x86_64.

Instruction for Monero compilation:
 - [Ubuntu 14.04 x86_64](http://moneroexamples.github.io/compile-monero-ubuntu/)
 - [Ubuntu 15.10 x86_64](http://moneroexamples.github.io/compile-monero-ubuntu-1510/)

Monero source code compilation and setup are same as
[here](http://moneroexamples.github.io/access-blockchain-in-cpp/).



## C++ code
The main part of the example is main.cpp.

```c++
int main(int ac, const char* av[]) {

    // get command line options
    xmreg::CmdLineOptions opts {ac, av};

    auto help_opt = opts.get_option<bool>("help");

    // if help was chosen, display help text and finish
    if (*help_opt)
    {
        return 0;
    }


    // flag indicating if viewkey and address were
    // given by the user
    bool VIEWKEY_AND_ADDRESS_GIVEN {false};

    // get other options
    auto tx_hash_opt = opts.get_option<string>("txhash");
    auto viewkey_opt = opts.get_option<string>("viewkey");
    auto address_opt = opts.get_option<string>("address");
    auto bc_path_opt = opts.get_option<string>("bc-path");


    // get the program command line options, or
    // some default values for quick check
    string tx_hash_str = tx_hash_opt ? *tx_hash_opt : "09d9e8eccf82b3d6811ed7005102caf1b605f325cf60ed372abeb4a67d956fff";


    crypto::hash tx_hash;

    if (!xmreg::parse_str_secret_key(tx_hash_str, tx_hash))
    {
        cerr << "Cant parse tx hash: " << tx_hash_str << endl;
        return 1;
    }

    crypto::secret_key private_view_key;
    cryptonote::account_public_address address;

    if (viewkey_opt && address_opt)
    {
         // parse string representing given private viewkey
        if (!xmreg::parse_str_secret_key(*viewkey_opt, private_view_key))
        {
            cerr << "Cant parse view key: " << *viewkey_opt << endl;
            return 1;
        }

        // parse string representing given monero address
        if (!xmreg::parse_str_address(*address_opt,  address))
        {
            cerr << "Cant parse address: " << *address_opt << endl;
            return 1;
        }

        VIEWKEY_AND_ADDRESS_GIVEN = true;
    }


    path blockchain_path;

    if (!xmreg::get_blockchain_path(bc_path_opt, blockchain_path))
    {
        // if problem obtaining blockchain path, finish.
        return 1;
    }

    print("Blockchain path      : {}\n", blockchain_path);

    // enable basic monero log output
    xmreg::enable_monero_log();

    // create instance of our MicroCore
    xmreg::MicroCore mcore;

    // initialize the core using the blockchain path
    if (!mcore.init(blockchain_path.string()))
    {
        cerr << "Error accessing blockchain." << endl;
        return 1;
    }


    print("\n\ntx hash          : {}\n\n", tx_hash);

    if (VIEWKEY_AND_ADDRESS_GIVEN)
    {
        // lets check our keys
        print("private view key : {}\n", private_view_key);
        print("address          : {}\n\n\n", address);
    }


    // get the high level cryptonote::Blockchain object to interact
    // with the blockchain lmdb database
    cryptonote::Blockchain& core_storage = mcore.get_core();

    cryptonote::transaction tx;

    try
    {
        // get transaction with given hash
        tx = core_storage.get_db().get_tx(tx_hash);
    }
    catch (const std::exception& e)
    {
        cerr << e.what() << endl;
        return false;
    }

    for (const cryptonote::txin_v& tx_in: tx.vin)
    {
        // get tx input key
        const cryptonote::txin_to_key& tx_in_to_key
                = boost::get<cryptonote::txin_to_key>(tx_in);


        print("Input's Key image: {}, xmr: {:0.4f}\n",
              tx_in_to_key.k_image,
              xmreg::get_xmr(tx_in_to_key.amount));

        // get absolute offsets of mixins
        std::vector<uint64_t> absolute_offsets
                = cryptonote::relative_output_offsets_to_absolute(
                        tx_in_to_key.key_offsets);

        std::vector<cryptonote::output_data_t> outputs;
        core_storage.get_db().get_output_key(tx_in_to_key.amount,
                                             absolute_offsets,
                                             outputs);

        size_t count = 0;

        for (const uint64_t& i: absolute_offsets)
        {
            cryptonote::output_data_t output_data;

            // get tx hash and output index for output
            if (count < outputs.size())
            {
                output_data = outputs.at(count);
            }
            else
            {
                output_data = core_storage.get_db().get_output_key(
                        tx_in_to_key.amount, i);
            }


            // find tx_hash with given output
            crypto::hash tx_hash;
            cryptonote::transaction tx_found;

            if (!mcore.get_tx_hash_from_output_pubkey(
                    output_data.pubkey,
                    output_data.height,
                    tx_hash, tx_found))
            {
                print("- cant find tx_hash for ouput: {}, mixin no: {}, blk: {}\n",
                      output_data.pubkey, count + 1, output_data.height);

                continue;
            }


            // find output in a given transaction
            // basted on its public key
            cryptonote::tx_out found_output;
            size_t output_index;

            if (!mcore.find_output_in_tx(tx_found,
                                         output_data.pubkey,
                                         found_output,
                                         output_index))
            {
                print("- cant find tx_out for ouput: {}, mixin no: {}, blk: {}\n",
                      output_data.pubkey, count + 1, output_data.height);

                continue;
            }

            print("\n - mixin no: {}, block height: {}",
                  count + 1, output_data.height);

            bool is_ours {false};

            if (VIEWKEY_AND_ADDRESS_GIVEN)
            {
                // check if the given mixin's output is ours based
                // on the view key and public spend key from the address
                is_ours = xmreg::is_output_ours(output_index, tx_found,
                                                private_view_key,
                                                address.m_spend_public_key);

                Color c  = is_ours ? Color::GREEN : Color::RED;

                print(", ours: "); print_colored(c, "{}", is_ours);
            }

            print("\n  - output's pubkey: {}\n", output_data.pubkey);

            print("  - in tx with hash: {}, out_i: {:03d}, xmr: {:0.4f}\n",
                  tx_hash, output_index, xmreg::get_xmr(found_output.amount));

            ++count;
        }

        cout << endl;
    }

    cout << "\nEnd of program." << endl;

    return 0;
}
```

## Program options

```
./showmixins -h
showmixins, shows mixin outputs used for each input in a given transaction:
  -h [ --help ] [=arg(=1)] (=0) produce help message
  -t [ --txhash ] arg           transaction hash
  -v [ --viewkey ] arg          private view key string
  -a [ --address ] arg          monero address string
  -b [ --bc-path ] arg          path to lmdb blockchain
```

## Example input and output 1
Just a transaction hash given, without corresponding address and view key.

```bash
./showmixins -t 49503c381ed74da2079697f0e8b7228608da3cade22575774ab8cf5ca425c3fe
```
Output:
```bash
tx hash          : <49503c381ed74da2079697f0e8b7228608da3cade22575774ab8cf5ca425c3fe>

Input's Key image: <54802347b456a6dd632aea85cf970b09244107b6d5cea924feb7deafdc37cf9d>, xmr: 1.0000

 - mixin no: 1, block height: 67326
  - output's pubkey: <9f3145e43d7e0e3bbeb57d5a2fafef952d315bac341e507645621ed86efd1155>
  - in tx with hash: <21885df01a25c548ddc0bb26dacba7fcc63f8c2810e193d4048fccb9791b1b38>, out_i: 175, xmr: 1.0000

 - mixin no: 2, block height: 143814
  - output's pubkey: <eeaa6f0a7cc1e975815743f12434b81a921f96c650fe269159dae20ef4077061>
  - in tx with hash: <56aabcd79cd2c063bd40636a2ca69e9933c95e93ec3ae15e93beafbae0293a83>, out_i: 039, xmr: 1.0000

 - mixin no: 3, block height: 153000
  - output's pubkey: <ad89c7b09b1f02ad872270510ccb96d24252170cd01bbbb45b307daf89e7ee5c>
  - in tx with hash: <0daef7d911fc62ae50ee0134cb247fcf97061091fcbb1fcf4d96d1c9cdb8a969>, out_i: 036, xmr: 1.0000

 - mixin no: 4, block height: 168055
  - output's pubkey: <623eba3d75cc706f34b62c50cc212267e86f50df123741f1deea039da04b6a4e>
  - in tx with hash: <31aa16467530036597f50f07fc30c3c8f8a8df55b19de831fcf3d2c18a951e1f>, out_i: 039, xmr: 1.0000

 - mixin no: 5, block height: 277037
  - output's pubkey: <f8f1ac2151024887959891d6021d84e6289a02b8743b84f941df15e387fc7c95>
  - in tx with hash: <fc27fd68ca723784c7b9e0459cc9a53779bd1bd317f5a255114d897ecbed7464>, out_i: 202, xmr: 1.0000

 - mixin no: 6, block height: 539756
  - output's pubkey: <b81195393cc497f4deff5dbd9f6fc04d4c492e6c523aadb40dbd29cdf38d4662>
  - in tx with hash: <6143d5d3f5ada27c1eddc46176607b67f492b1e9427757174e3e3215eeadaf63>, out_i: 049, xmr: 1.0000

 - mixin no: 7, block height: 844912
  - output's pubkey: <852e2f8c919988294a15a65a4be8adff70e14c5907b0fee7e2ee005c134345e4>
  - in tx with hash: <27003d209ae8854a72735e0cb14f46879dafbac65cf593fb880926a2a674efce>, out_i: 002, xmr: 1.0000
```

## Example input and output 2
Transaction hash given along with the corresponding address and view key.

```bash
./showmixins -a 42SLnbz1Ym9YtU3zHwKbQL8hXnGhtQ23BdnTGBtWwj6AXHSdEqRikDbM3wQxDWMhyCKZbQ9TfFh9N1SvHMXT81kK7senkME -v bdee822e89095833315925543a6d5b2a2a4418815cdfb3d0e91722d9c0b79501 -t 49503c381ed74da2079697f0e8b7228608da3cade22575774ab8cf5ca425c3fe
```

Result:
```bash
tx hash          : <49503c381ed74da2079697f0e8b7228608da3cade22575774ab8cf5ca425c3fe>

private view key : <bdee822e89095833315925543a6d5b2a2a4418815cdfb3d0e91722d9c0b79501>
address          : <42SLnbz1Ym9YtU3zHwKbQL8hXnGhtQ23BdnTGBtWwj6AXHSdEqRikDbM3wQxDWMhyCKZbQ9TfFh9N1SvHMXT81kK7senkME>


Input's Key image: <54802347b456a6dd632aea85cf970b09244107b6d5cea924feb7deafdc37cf9d>, xmr: 1.0000

 - mixin no: 1, block height: 67326, ours: false
  - output's pubkey: <9f3145e43d7e0e3bbeb57d5a2fafef952d315bac341e507645621ed86efd1155>
  - in tx with hash: <21885df01a25c548ddc0bb26dacba7fcc63f8c2810e193d4048fccb9791b1b38>, out_i: 175, xmr: 1.0000

 - mixin no: 2, block height: 143814, ours: false
  - output's pubkey: <eeaa6f0a7cc1e975815743f12434b81a921f96c650fe269159dae20ef4077061>
  - in tx with hash: <56aabcd79cd2c063bd40636a2ca69e9933c95e93ec3ae15e93beafbae0293a83>, out_i: 039, xmr: 1.0000

 - mixin no: 3, block height: 153000, ours: false
  - output's pubkey: <ad89c7b09b1f02ad872270510ccb96d24252170cd01bbbb45b307daf89e7ee5c>
  - in tx with hash: <0daef7d911fc62ae50ee0134cb247fcf97061091fcbb1fcf4d96d1c9cdb8a969>, out_i: 036, xmr: 1.0000

 - mixin no: 4, block height: 168055, ours: false
  - output's pubkey: <623eba3d75cc706f34b62c50cc212267e86f50df123741f1deea039da04b6a4e>
  - in tx with hash: <31aa16467530036597f50f07fc30c3c8f8a8df55b19de831fcf3d2c18a951e1f>, out_i: 039, xmr: 1.0000

 - mixin no: 5, block height: 277037, ours: false
  - output's pubkey: <f8f1ac2151024887959891d6021d84e6289a02b8743b84f941df15e387fc7c95>
  - in tx with hash: <fc27fd68ca723784c7b9e0459cc9a53779bd1bd317f5a255114d897ecbed7464>, out_i: 202, xmr: 1.0000

 - mixin no: 6, block height: 539756, ours: false
  - output's pubkey: <b81195393cc497f4deff5dbd9f6fc04d4c492e6c523aadb40dbd29cdf38d4662>
  - in tx with hash: <6143d5d3f5ada27c1eddc46176607b67f492b1e9427757174e3e3215eeadaf63>, out_i: 049, xmr: 1.0000

 - mixin no: 7, block height: 844912, ours: true
  - output's pubkey: <852e2f8c919988294a15a65a4be8adff70e14c5907b0fee7e2ee005c134345e4>
  - in tx with hash: <27003d209ae8854a72735e0cb14f46879dafbac65cf593fb880926a2a674efce>, out_i: 002, xmr: 1.0000
```



## Example input and output 2
Transaction hash given along with the corresponding address and view key.

```bash
./showmixins -a 41vEA7Ye8Bpeda6g59v5t46koWrVn2PNgEKgzquJjmiKCFTsh9gajr8J3pad49rqu581TAtFGCH9CYTCkYrCpuWUG9GkgeB -v fed77158ec692fe9eb951f6aeb22c3bda16fe8926c1aac13a5651a9c27f34309  -t 33fbb14119e1777f6577b08188031f8b60e448cc23135d37bda8aee6b1d97d97
```

```bash
tx hash          : <33fbb14119e1777f6577b08188031f8b60e448cc23135d37bda8aee6b1d97d97>

private view key : <fed77158ec692fe9eb951f6aeb22c3bda16fe8926c1aac13a5651a9c27f34309>
address          : <41vEA7Ye8Bpeda6g59v5t46koWrVn2PNgEKgzquJjmiKCFTsh9gajr8J3pad49rqu581TAtFGCH9CYTCkYrCpuWUG9GkgeB>


Input's Key image: <059c860753ac88c1ee2abd7531169ebd636b0f9026ee1d288590f0eaf291cfce>, xmr: 0.0700

 - mixin no: 1, block height: 100587, ours: false
  - output's pubkey: <899dbdf74d92ab761d43b9e64881a563d233c352284246dbad856164bce10c56>
  - in tx with hash: <587078cf170458879dfa0f36f1acb5ab8aa3a4757d3159e73364cc81a2604d55>, out_i: 087, xmr: 0.0700

 - mixin no: 2, block height: 196153, ours: false
  - output's pubkey: <40cb6dfca1486763fdeb78589b8497e14b0aace19b8042d0fb3b1605c0659ebf>
  - in tx with hash: <d0b3a8cb3144efde755ff1f0a53da08141612a614c76b7045ab449a583dc6abc>, out_i: 000, xmr: 0.0700

 - mixin no: 3, block height: 419000, ours: false
  - output's pubkey: <1f4e5e32dfb1bf9e51e3858be87b8cdc20ca79b1507b51070cbac3fcf474c84a>
  - in tx with hash: <2e07b521ff7095e4b9c3458b8b406914e971a860182d63687fc9f7030d1598b5>, out_i: 005, xmr: 0.0700

 - mixin no: 4, block height: 442769, ours: false
  - output's pubkey: <2c1bf7733129f520e24349f9da1c0dbadfe6faa40020ac4803d516bc2c654ee4>
  - in tx with hash: <88639977921a2dbb7fa9a3d50aedadd49627957f3d726f47342255d26c9d6af1>, out_i: 001, xmr: 0.0700

 - mixin no: 5, block height: 536199, ours: false
  - output's pubkey: <099a1522e0c1686f6e7d3739c985949d113a228f297f61ebbcc3430e77072047>
  - in tx with hash: <1783b473caa464b96e2abea1ce6ef0f6047aecc69421b1e3f7677e329be13340>, out_i: 001, xmr: 0.0700

 - mixin no: 6, block height: 597457, ours: false
  - output's pubkey: <6a893bb20ca12a2debb0bc4f6905962cf78a034b8c32cad241a1718fe9cda04d>
  - in tx with hash: <bf22f3291dc7b7f3705a16af4dc725aded208bd63a34fd6df721ad55825ff7c0>, out_i: 001, xmr: 0.0700

 - mixin no: 7, block height: 619035, ours: false
  - output's pubkey: <5d898066818e39e9521457e317d930fde698c81c0199112025f68eef01d12419>
  - in tx with hash: <7db677dafa7a83ce6d056e6cf7b2631ac0376e873ef0c34eed4795b66cd3476a>, out_i: 001, xmr: 0.0700

 - mixin no: 8, block height: 629805, ours: false
  - output's pubkey: <fc926f529076df29b38396fa8b505026e5b4c59717d675df6913146837c3d49a>
  - in tx with hash: <94421fdf1fcd190d9f01ef27706ee3cf1a6de550522e0a41118f8a62e1dcad67>, out_i: 001, xmr: 0.0700

 - mixin no: 9, block height: 719664, ours: false
  - output's pubkey: <3897f183edbc6863ad17c244e5f7847c53456ee6dd96ee0f7e8c69276355a088>
  - in tx with hash: <64f0cc93acbea3f387c6ecc6e863facf2a14a25eec15902e0eda2e466f4dd8b9>, out_i: 001, xmr: 0.0700

 - mixin no: 10, block height: 802102, ours: false
  - output's pubkey: <bb7357d4a834707e3e539b4d58522651103deceaaf2cfadffa4e834e7778fd89>
  - in tx with hash: <0173c66a1fbd2a8b659e3e01ac4c82da1e9b45a0fac59ac566a9b13e51c90699>, out_i: 001, xmr: 0.0700

 - mixin no: 11, block height: 844908, ours: true
  - output's pubkey: <848a4a76a1d2652fa103ceea9bdd7d4c8a2fcd54cd7050648fa57bb144239642>
  - in tx with hash: <ce26c93d631ff7876134eeaeb574f6e9c791fe3eb816de16f68ee2138c9ccdbb>, out_i: 001, xmr: 0.0700
```

## Compile this example
The dependencies are same as those for Monero, so I assume Monero compiles
correctly. If so then to download and compile this example, the following
steps can be executed:

```bash
# download the source code
git clone https://github.com/moneroexamples/show-mixins-for-tx.git

# enter the downloaded sourced code folder
cd show-mixins-for-tx

# create the makefile
cmake .

# compile
make
```

After this, `showmixins` executable file should be present in access-blockchain-in-cpp
folder. How to use it, can be seen in the above example outputs.


## How can you help?

Constructive criticism, code and website edits are always good. They can be made through github.

Some Monero are also welcome:
```
48daf1rG3hE1Txapcsxh6WXNe9MLNKtu7W7tKTivtSoVLHErYzvdcpea2nSTgGkz66RFP4GKVAsTV14v6G3oddBTHfxP6tU
```
