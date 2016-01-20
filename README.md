# Show mixins in a transaction

Mixins represent one of the main
advantages of [Monero](https://getmonero.org/) over other cryptocurrencies.
[http://moneroblocks.eu/](http://moneroblocks.eu/) shows mixins used
for each transaction, but it lacks the ability to identify the real mixin
based on the address and viewkey provided, and also it provides very limited
information about the mixins.

In this example, those limitations are addressed. Specifically, a C++ program called
`showmixins` is developed. The program prints out detailed information about
mixins used in a given transaction.  In addition, if monero address and the
corresponding view key are provided, the true mixin in each input is identified.

## Prerequisite

Everything here was done and tested using Monero 0.9.1 on
Xubuntu 15.10 x86_64.

Instruction for Monero 0.9 compilation:
 - [Ubuntu 14.04 and 15.10 x86_64](https://github.com/moneroexamples/compile-monero-09-on-ubuntu)

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


        print("Input's Key image: {}, xmr: {:0.6f}\n",
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

            bool output_at {true};

            // get tx hash and output index for output
            if (count < outputs.size())
            {
                output_data = outputs.at(count);
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


            // get global transaction index in the blockchain
            vector<uint64_t> out_global_indeces;

            if (!core_storage.get_tx_outputs_gindexs(
                    cryptonote::get_transaction_hash(tx_found),
                    out_global_indeces))
            {
                print("- cant find global indices for tx: {}\n", tx_hash);

                continue;
            }

            // get the global index for the current output
            uint64_t global_out_idx = {0};

            if (output_index < out_global_indeces.size())
            {
                global_out_idx = out_global_indeces[output_index];
            }


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

            // get tx public key from extras field
            crypto::public_key pub_tx_key = cryptonote::get_tx_pub_key_from_extra(tx_found);

            print("\n"
                  "  - output's pubkey: {}\n", output_data.pubkey);

            print("  - in tx with hash: {}\n", tx_hash);

            print("  - this tx pub key: {}\n", pub_tx_key);

            print("  - out_i: {:03d}, g_idx: {:d}, xmr: {:0.6f}\n",
                  output_index, global_out_idx, xmreg::get_xmr(found_output.amount));

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

## Example input and output 1 (mixin 6)
Just a transaction hash given, without corresponding address and view key.

```bash
./showmixins 49503c381ed74da2079697f0e8b7228608da3cade22575774ab8cf5ca425c3fe
```
Output:
```bash
tx hash          : <49503c381ed74da2079697f0e8b7228608da3cade22575774ab8cf5ca425c3fe>

Input's Key image: <54802347b456a6dd632aea85cf970b09244107b6d5cea924feb7deafdc37cf9d>, xmr: 1.000000

 - mixin no: 1, block height: 67326, timestamp: 2014-06-02 16:58:23
  - output's pubkey: <9f3145e43d7e0e3bbeb57d5a2fafef952d315bac341e507645621ed86efd1155>
  - in tx with hash: <21885df01a25c548ddc0bb26dacba7fcc63f8c2810e193d4048fccb9791b1b38>
  - this tx pub key: <6870bc701bea76bd66174216450eaae37166b080d1e7c3db1ffb3d760316f98c>
  - out_i: 175, g_idx: 8381, xmr: 1.000000

 - mixin no: 2, block height: 143814, timestamp: 2014-07-25 13:31:17
  - output's pubkey: <eeaa6f0a7cc1e975815743f12434b81a921f96c650fe269159dae20ef4077061>
  - in tx with hash: <56aabcd79cd2c063bd40636a2ca69e9933c95e93ec3ae15e93beafbae0293a83>
  - this tx pub key: <ded232fda057e79ecc6353a6a2fd34a3cd0a22053123aa6e35f839f8715b2d99>
  - out_i: 039, g_idx: 132680, xmr: 1.000000

 - mixin no: 3, block height: 153000, timestamp: 2014-07-31 21:51:33
  - output's pubkey: <ad89c7b09b1f02ad872270510ccb96d24252170cd01bbbb45b307daf89e7ee5c>
  - in tx with hash: <0daef7d911fc62ae50ee0134cb247fcf97061091fcbb1fcf4d96d1c9cdb8a969>
  - this tx pub key: <d841906d5a04d72d4585bd37c23f3501f645f407a392c86d1aaf00ee25842c86>
  - out_i: 036, g_idx: 153950, xmr: 1.000000

 - mixin no: 4, block height: 168055, timestamp: 2014-08-11 11:27:23
  - output's pubkey: <623eba3d75cc706f34b62c50cc212267e86f50df123741f1deea039da04b6a4e>
  - in tx with hash: <31aa16467530036597f50f07fc30c3c8f8a8df55b19de831fcf3d2c18a951e1f>
  - this tx pub key: <144ce215672c8067b8dc172c2b04ac3e11dfc5fcc027c8ed19b327550dfce532>
  - out_i: 039, g_idx: 180712, xmr: 1.000000

 - mixin no: 5, block height: 277037, timestamp: 2014-10-26 13:04:02
  - output's pubkey: <f8f1ac2151024887959891d6021d84e6289a02b8743b84f941df15e387fc7c95>
  - in tx with hash: <fc27fd68ca723784c7b9e0459cc9a53779bd1bd317f5a255114d897ecbed7464>
  - this tx pub key: <d535a1fa0eb144794759b6e3adbc6389fce8e04bf172dc08c9dcc2325cb44975>
  - out_i: 202, g_idx: 316577, xmr: 1.000000

 - mixin no: 6, block height: 539756, timestamp: 2015-04-28 08:58:05
  - output's pubkey: <b81195393cc497f4deff5dbd9f6fc04d4c492e6c523aadb40dbd29cdf38d4662>
  - in tx with hash: <6143d5d3f5ada27c1eddc46176607b67f492b1e9427757174e3e3215eeadaf63>
  - this tx pub key: <b458cc6c05f38339a33036c3ed992997a7511b6148256c6ddf2d6742e9f4c282>
  - out_i: 049, g_idx: 547897, xmr: 1.000000

 - mixin no: 7, block height: 844912, timestamp: 2015-11-28 11:22:49
  - output's pubkey: <852e2f8c919988294a15a65a4be8adff70e14c5907b0fee7e2ee005c134345e4>
  - in tx with hash: <27003d209ae8854a72735e0cb14f46879dafbac65cf593fb880926a2a674efce>
  - this tx pub key: <70fca5b6be58dbe39ed3fc229bb2a11ccceea89073591a18749c82f816182b37>
  - out_i: 002, g_idx: 654842, xmr: 1.000000
```

## Example input and output 2 (mixin 6)
Transaction hash given along with the corresponding address and view key.

```bash
./showmixins -a 42SLnbz1Ym9YtU3zHwKbQL8hXnGhtQ23BdnTGBtWwj6AXHSdEqRikDbM3wQxDWMhyCKZbQ9TfFh9N1SvHMXT81kK7senkME -v bdee822e89095833315925543a6d5b2a2a4418815cdfb3d0e91722d9c0b79501 -t 49503c381ed74da2079697f0e8b7228608da3cade22575774ab8cf5ca425c3fe
```

Result:
```bash
tx hash          : <49503c381ed74da2079697f0e8b7228608da3cade22575774ab8cf5ca425c3fe>

private view key : <bdee822e89095833315925543a6d5b2a2a4418815cdfb3d0e91722d9c0b79501>
address          : <42SLnbz1Ym9YtU3zHwKbQL8hXnGhtQ23BdnTGBtWwj6AXHSdEqRikDbM3wQxDWMhyCKZbQ9TfFh9N1SvHMXT81kK7senkME>


Input's Key image: <54802347b456a6dd632aea85cf970b09244107b6d5cea924feb7deafdc37cf9d>, xmr: 1.000000

 - mixin no: 1, block height: 67326, timestamp: 2014-06-02 16:58:23, ours: false
  - output's pubkey: <9f3145e43d7e0e3bbeb57d5a2fafef952d315bac341e507645621ed86efd1155>
  - in tx with hash: <21885df01a25c548ddc0bb26dacba7fcc63f8c2810e193d4048fccb9791b1b38>
  - this tx pub key: <6870bc701bea76bd66174216450eaae37166b080d1e7c3db1ffb3d760316f98c>
  - out_i: 175, g_idx: 8381, xmr: 1.000000

 - mixin no: 2, block height: 143814, timestamp: 2014-07-25 13:31:17, ours: false
  - output's pubkey: <eeaa6f0a7cc1e975815743f12434b81a921f96c650fe269159dae20ef4077061>
  - in tx with hash: <56aabcd79cd2c063bd40636a2ca69e9933c95e93ec3ae15e93beafbae0293a83>
  - this tx pub key: <ded232fda057e79ecc6353a6a2fd34a3cd0a22053123aa6e35f839f8715b2d99>
  - out_i: 039, g_idx: 132680, xmr: 1.000000

 - mixin no: 3, block height: 153000, timestamp: 2014-07-31 21:51:33, ours: false
  - output's pubkey: <ad89c7b09b1f02ad872270510ccb96d24252170cd01bbbb45b307daf89e7ee5c>
  - in tx with hash: <0daef7d911fc62ae50ee0134cb247fcf97061091fcbb1fcf4d96d1c9cdb8a969>
  - this tx pub key: <d841906d5a04d72d4585bd37c23f3501f645f407a392c86d1aaf00ee25842c86>
  - out_i: 036, g_idx: 153950, xmr: 1.000000

 - mixin no: 4, block height: 168055, timestamp: 2014-08-11 11:27:23, ours: false
  - output's pubkey: <623eba3d75cc706f34b62c50cc212267e86f50df123741f1deea039da04b6a4e>
  - in tx with hash: <31aa16467530036597f50f07fc30c3c8f8a8df55b19de831fcf3d2c18a951e1f>
  - this tx pub key: <144ce215672c8067b8dc172c2b04ac3e11dfc5fcc027c8ed19b327550dfce532>
  - out_i: 039, g_idx: 180712, xmr: 1.000000

 - mixin no: 5, block height: 277037, timestamp: 2014-10-26 13:04:02, ours: false
  - output's pubkey: <f8f1ac2151024887959891d6021d84e6289a02b8743b84f941df15e387fc7c95>
  - in tx with hash: <fc27fd68ca723784c7b9e0459cc9a53779bd1bd317f5a255114d897ecbed7464>
  - this tx pub key: <d535a1fa0eb144794759b6e3adbc6389fce8e04bf172dc08c9dcc2325cb44975>
  - out_i: 202, g_idx: 316577, xmr: 1.000000

 - mixin no: 6, block height: 539756, timestamp: 2015-04-28 08:58:05, ours: false
  - output's pubkey: <b81195393cc497f4deff5dbd9f6fc04d4c492e6c523aadb40dbd29cdf38d4662>
  - in tx with hash: <6143d5d3f5ada27c1eddc46176607b67f492b1e9427757174e3e3215eeadaf63>
  - this tx pub key: <b458cc6c05f38339a33036c3ed992997a7511b6148256c6ddf2d6742e9f4c282>
  - out_i: 049, g_idx: 547897, xmr: 1.000000

 - mixin no: 7, block height: 844912, timestamp: 2015-11-28 11:22:49, ours: true
  - output's pubkey: <852e2f8c919988294a15a65a4be8adff70e14c5907b0fee7e2ee005c134345e4>
  - in tx with hash: <27003d209ae8854a72735e0cb14f46879dafbac65cf593fb880926a2a674efce>
  - this tx pub key: <70fca5b6be58dbe39ed3fc229bb2a11ccceea89073591a18749c82f816182b37>
  - out_i: 002, g_idx: 654842, xmr: 1.000000
```



## Example input and output 3 (mixin 10)
Transaction hash given along with the corresponding address and view key.

```bash
./showmixins -a 41vEA7Ye8Bpeda6g59v5t46koWrVn2PNgEKgzquJjmiKCFTsh9gajr8J3pad49rqu581TAtFGCH9CYTCkYrCpuWUG9GkgeB -v fed77158ec692fe9eb951f6aeb22c3bda16fe8926c1aac13a5651a9c27f34309  -t 33fbb14119e1777f6577b08188031f8b60e448cc23135d37bda8aee6b1d97d97
```
Result:
```bash
tx hash          : <33fbb14119e1777f6577b08188031f8b60e448cc23135d37bda8aee6b1d97d97>

private view key : <fed77158ec692fe9eb951f6aeb22c3bda16fe8926c1aac13a5651a9c27f34309>
address          : <41vEA7Ye8Bpeda6g59v5t46koWrVn2PNgEKgzquJjmiKCFTsh9gajr8J3pad49rqu581TAtFGCH9CYTCkYrCpuWUG9GkgeB>


Input's Key image: <059c860753ac88c1ee2abd7531169ebd636b0f9026ee1d288590f0eaf291cfce>, xmr: 0.070000

 - mixin no: 1, block height: 100587, timestamp: 2014-06-25 14:43:19, ours: false
  - output's pubkey: <899dbdf74d92ab761d43b9e64881a563d233c352284246dbad856164bce10c56>
  - in tx with hash: <587078cf170458879dfa0f36f1acb5ab8aa3a4757d3159e73364cc81a2604d55>
  - this tx pub key: <fba03d9a2edbcd11526d01653c57d3b097521bcd68877ff3d4f3c541d9cac823>
  - out_i: 087, g_idx: 76852, xmr: 0.070000

 - mixin no: 2, block height: 196153, timestamp: 2014-08-30 22:12:17, ours: false
  - output's pubkey: <40cb6dfca1486763fdeb78589b8497e14b0aace19b8042d0fb3b1605c0659ebf>
  - in tx with hash: <d0b3a8cb3144efde755ff1f0a53da08141612a614c76b7045ab449a583dc6abc>
  - this tx pub key: <df553ee9455ce202924f7dc2a00946a810f3764203727a32ca7c71dea769ff94>
  - out_i: 000, g_idx: 103052, xmr: 0.070000

 - mixin no: 3, block height: 419000, timestamp: 2015-02-03 01:01:58, ours: false
  - output's pubkey: <1f4e5e32dfb1bf9e51e3858be87b8cdc20ca79b1507b51070cbac3fcf474c84a>
  - in tx with hash: <2e07b521ff7095e4b9c3458b8b406914e971a860182d63687fc9f7030d1598b5>
  - this tx pub key: <8ceb3b90515ff6542f1e5e72d18dee22cc4d3bf4da72f236c453e07cc99cb283>
  - out_i: 005, g_idx: 115440, xmr: 0.070000

 - mixin no: 4, block height: 442769, timestamp: 2015-02-19 16:29:28, ours: false
  - output's pubkey: <2c1bf7733129f520e24349f9da1c0dbadfe6faa40020ac4803d516bc2c654ee4>
  - in tx with hash: <88639977921a2dbb7fa9a3d50aedadd49627957f3d726f47342255d26c9d6af1>
  - this tx pub key: <3b386cfeaee3de991540e0694c40e00126ad3eba96b68491de27a44734344725>
  - out_i: 001, g_idx: 117822, xmr: 0.070000

 - mixin no: 5, block height: 536199, timestamp: 2015-04-25 20:12:12, ours: false
  - output's pubkey: <099a1522e0c1686f6e7d3739c985949d113a228f297f61ebbcc3430e77072047>
  - in tx with hash: <1783b473caa464b96e2abea1ce6ef0f6047aecc69421b1e3f7677e329be13340>
  - this tx pub key: <d57322ca42afb2c27dea21e668f9269586abbec11097c2bf2507723934b5c579>
  - out_i: 001, g_idx: 129931, xmr: 0.070000

 - mixin no: 6, block height: 597457, timestamp: 2015-06-07 16:36:38, ours: false
  - output's pubkey: <6a893bb20ca12a2debb0bc4f6905962cf78a034b8c32cad241a1718fe9cda04d>
  - in tx with hash: <bf22f3291dc7b7f3705a16af4dc725aded208bd63a34fd6df721ad55825ff7c0>
  - this tx pub key: <6e241f6031b09070b1d4cde06d3770bab4c6e4b11a340dacad983ce31fc0af92>
  - out_i: 001, g_idx: 137977, xmr: 0.070000

 - mixin no: 7, block height: 619035, timestamp: 2015-06-22 19:01:54, ours: false
  - output's pubkey: <5d898066818e39e9521457e317d930fde698c81c0199112025f68eef01d12419>
  - in tx with hash: <7db677dafa7a83ce6d056e6cf7b2631ac0376e873ef0c34eed4795b66cd3476a>
  - this tx pub key: <9081f7320f046fa51e077d56bb8c3dc305191aefba9a80c09e9f310a84689317>
  - out_i: 001, g_idx: 140897, xmr: 0.070000

 - mixin no: 8, block height: 629805, timestamp: 2015-06-30 08:11:13, ours: false
  - output's pubkey: <fc926f529076df29b38396fa8b505026e5b4c59717d675df6913146837c3d49a>
  - in tx with hash: <94421fdf1fcd190d9f01ef27706ee3cf1a6de550522e0a41118f8a62e1dcad67>
  - this tx pub key: <051c018a4564c6ecc6dfa021737117c2c5ba3dfadcbe3b63c02d040233264c96>
  - out_i: 001, g_idx: 142288, xmr: 0.070000

 - mixin no: 9, block height: 719664, timestamp: 2015-09-01 06:28:33, ours: false
  - output's pubkey: <3897f183edbc6863ad17c244e5f7847c53456ee6dd96ee0f7e8c69276355a088>
  - in tx with hash: <64f0cc93acbea3f387c6ecc6e863facf2a14a25eec15902e0eda2e466f4dd8b9>
  - this tx pub key: <8e3cce1cfdc1692264bd93a6f9fc2c66deb1642a14c50293fd92c85eed828710>
  - out_i: 001, g_idx: 153185, xmr: 0.070000

 - mixin no: 10, block height: 802102, timestamp: 2015-10-29 03:54:39, ours: false
  - output's pubkey: <bb7357d4a834707e3e539b4d58522651103deceaaf2cfadffa4e834e7778fd89>
  - in tx with hash: <0173c66a1fbd2a8b659e3e01ac4c82da1e9b45a0fac59ac566a9b13e51c90699>
  - this tx pub key: <e157acc67cbbcfba9c7733fc0c3667e27f8b869998d428b85d268933bfec8e62>
  - out_i: 001, g_idx: 164180, xmr: 0.070000

 - mixin no: 11, block height: 844908, timestamp: 2015-11-28 11:15:05, ours: true
  - output's pubkey: <848a4a76a1d2652fa103ceea9bdd7d4c8a2fcd54cd7050648fa57bb144239642>
  - in tx with hash: <ce26c93d631ff7876134eeaeb574f6e9c791fe3eb816de16f68ee2138c9ccdbb>
  - this tx pub key: <8c730462989bab83bafbfe139ec3603ee47831bd0212185484ac6c22c8a63307>
  - out_i: 001, g_idx: 169601, xmr: 0.070000
```

## Example input and output 4 (mixin 3)
Transaction hash given along with the corresponding address and view key.

```bash
./showmixins -a 41vEA7Ye8Bpeda6g59v5t46koWrVn2PNgEKgzquJjmiKCFTsh9gajr8J3pad49rqu581TAtFGCH9CYTCkYrCpuWUG9GkgeB -v fed77158ec692fe9eb951f6aeb22c3bda16fe8926c1aac13a5651a9c27f34309  -t b7822d7ae83a07fa49ba54ff935fa6899a49fb5c35269369141470b709c073b8
```

```bash
tx hash          : <b7822d7ae83a07fa49ba54ff935fa6899a49fb5c35269369141470b709c073b8>

private view key : <fed77158ec692fe9eb951f6aeb22c3bda16fe8926c1aac13a5651a9c27f34309>
address          : <41vEA7Ye8Bpeda6g59v5t46koWrVn2PNgEKgzquJjmiKCFTsh9gajr8J3pad49rqu581TAtFGCH9CYTCkYrCpuWUG9GkgeB>


Input's Key image: <68d476d12c23ed8bacf9286469ccce41b3eb51eb3c6be2af5405764093f7e6e2>, xmr: 0.090000

 - mixin no: 1, block height: 108348, timestamp: 2014-06-30 23:22:48, ours: false
  - output's pubkey: <7b7d2d572efd85ada54fd7df8af054d04a28a6a0bfeaa7be60ec402faf076888>
  - in tx with hash: <dad81e7c0412ef9d02e308502b02ef2719798a5b2a0f4488a166e90e4e78211b>
  - this tx pub key: <39bcdd1ac483578c72f1875fa01dd3240e05e2fbae13ba9c3dbebdd2d81753c0>
  - out_i: 002, g_idx: 70913, xmr: 0.090000

 - mixin no: 2, block height: 131945, timestamp: 2014-07-17 08:39:12, ours: false
  - output's pubkey: <fff8d49e9e912a0ddbcc83a3d23284c4bf7041e9e560341261c2a70bc1db1079>
  - in tx with hash: <449c1642c8ba5c17b1994ad4069bf27499112915e57ff6d73fa1aba68a2af0e4>
  - this tx pub key: <420103c4028645547363f1b19988e29487c9a5c3d0d413e083468423c18bf0da>
  - out_i: 002, g_idx: 82215, xmr: 0.090000

 - mixin no: 3, block height: 485596, timestamp: 2015-03-21 13:11:54, ours: false
  - output's pubkey: <5f9e6cb693fdeb3521faf1a81460dde9ba9f89153dcfd5e3474931fc7c0dac48>
  - in tx with hash: <476ccffee2f6087348061782aa6fc583a8a80bd9d491eafd65c8979287efb24b>
  - this tx pub key: <b335c9c2e96b6b8d13f298dbc115d6751c79c56265b6669e6fe9c898dd3e89a3>
  - out_i: 000, g_idx: 144248, xmr: 0.090000

 - mixin no: 4, block height: 844131, timestamp: 2015-11-27 20:30:30, ours: true
  - output's pubkey: <5f217820bb13d6f29eb4e889ca85d4abd90411b9b4155bc441b31a159de86cf4>
  - in tx with hash: <d680c60ccc04cea643765428c7c44afb12e7b68cd50fd93182a86b8cb920f3f9>
  - this tx pub key: <b85cfe6c64012476c4a9969e15ed098e9ec5c66caf2c8d3786d16f5d0c4e131d>
  - out_i: 000, g_idx: 200232, xmr: 0.090000

Input's Key image: <7c2aeee6d01cfc71f6fc8b975269fb01b35a8934101672524c35275322a6d949>, xmr: 0.090000

 - mixin no: 1, block height: 115423, timestamp: 2014-07-05 20:58:00, ours: false
  - output's pubkey: <f63eb90fefea6abc08ab19a03f6a2b65f98496ed465bcd35532e3cfe0cbba718>
  - in tx with hash: <30726385586ffd850d8c1bf9fbfbfaedee197e30b3e5ddaf1d2c993153c013ae>
  - this tx pub key: <2140baf170b38744a5542e236cf0722b6af5b6d4f9c9399f93ce1d2340ee9b09>
  - out_i: 000, g_idx: 74485, xmr: 0.090000

 - mixin no: 2, block height: 472372, timestamp: 2015-03-12 07:12:23, ours: false
  - output's pubkey: <7e2120811ce322caf1981147eb3b7c03cda686d78dbc7367e5259b94f430319c>
  - in tx with hash: <a79749fa9eab22b38ef52d794bf3543982a191b6928640fc6cc0ea04e26e2956>
  - this tx pub key: <f684a30d0f33d59a31963a503345c688d30bb5f327623339288de405458c5de3>
  - out_i: 000, g_idx: 142309, xmr: 0.090000

 - mixin no: 3, block height: 735365, timestamp: 2015-09-12 07:14:00, ours: false
  - output's pubkey: <f0fc53e19d2a795dafa2566ab18819a54d9c7c98880bbf639af1eae8938ba01f>
  - in tx with hash: <e827c3d8ee2de1079a4f0aefb12f6b44d7100b2326ebf522640c126370578fdb>
  - this tx pub key: <3a165581f3e577582f4860c924bb7a8dabf89a3e8e1bef173d9a586e41e2c38e>
  - out_i: 002, g_idx: 183927, xmr: 0.090000

 - mixin no: 4, block height: 844131, timestamp: 2015-11-27 20:30:30, ours: true
  - output's pubkey: <5cb0a097d4f13062756ecd11c324c6c11eca2d8ebf90241bdc891a3ae08d1365>
  - in tx with hash: <c772702301bea767352f6309f0f46bf8b84a105580a6cdfdf30d16b073f3b2fe>
  - this tx pub key: <ef0a0ec9c3a3fd91d9ede9a2b6d7865464d952c0ec0fbfe6393cdeeda15753e7>
  - out_i: 000, g_idx: 200233, xmr: 0.090000
```



## Example input and output 5 (mixin 10)
```bash
./showmixins 3780e7ed251459a7547bd034c420029cc873eb59013bbd8c8d2ec745898ee83e -a 41vEA7Ye8Bpeda6g59v5t46koWrVn2PNgEKgzquJjmiKCFTsh9gajr8J3pad49rqu581TAtFGCH9CYTCkYrCpuWUG9GkgeB -v fed77158ec692fe9eb951f6aeb22c3bda16fe8926c1aac13a5651a9c27f34309
```

```bash
tx hash          : <3780e7ed251459a7547bd034c420029cc873eb59013bbd8c8d2ec745898ee83e>

private view key : <fed77158ec692fe9eb951f6aeb22c3bda16fe8926c1aac13a5651a9c27f34309>
address          : <41vEA7Ye8Bpeda6g59v5t46koWrVn2PNgEKgzquJjmiKCFTsh9gajr8J3pad49rqu581TAtFGCH9CYTCkYrCpuWUG9GkgeB>


Input's Key image: <efe0515308147f217ea55e550df811a7105e75a35cce9d8e1932d0506e172e50>, xmr: 0.090000

 - mixin no: 1, block height: 64823, timestamp: 2014-05-31 23:46:15, ours: false
  - output's pubkey: <88a218d7c5d63772b38c80c01c7ae89f0b2c929098ada089c1cec9f67ed67fcd>
  - in tx with hash: <fb454850d14e5c61a65998d4a2dedb3e3c2424a0bb1f84738e73af2defddc0b0>
  - this tx pub key: <a7f9e02246f054ccb541d0cf3423f39a796d3f00d86a3c0afdbda3171f338451>
  - out_i: 108, g_idx: 15127, xmr: 0.090000

 - mixin no: 2, block height: 127292, timestamp: 2014-07-14 04:20:11, ours: false
  - output's pubkey: <47688fbb17bc4ccfee9a38911d8103b8233cbe818771847123ae05e920174c9f>
  - in tx with hash: <79868f2fd9a4698dba0590e9034594c0560f8de12f5774412d40477d23fc2333>
  - this tx pub key: <cad49d4ada7ebee9b194e6431a53b140f891a5ad869a7bcffb6d00347c245cd9>
  - out_i: 000, g_idx: 79715, xmr: 0.090000

 - mixin no: 3, block height: 129928, timestamp: 2014-07-16 00:25:06, ours: false
  - output's pubkey: <592b53fa80024f18e3c99e9e6347721a0f310d0b01098bf109034e8427e5d81e>
  - in tx with hash: <ea94520c020f2b1d2e678829f61834bcbe51f0520a0819814d3001722dd6e2fa>
  - this tx pub key: <bbf9af02ca20224880f5954b2b9d0fbe55c3636c4d595186221952a1b7b560ab>
  - out_i: 001, g_idx: 81243, xmr: 0.090000

 - mixin no: 4, block height: 138504, timestamp: 2014-07-21 20:41:05, ours: false
  - output's pubkey: <966c9ca66cd95f0a14bb1b66d2d851ed1990d02771fd204ff6d9d65330116a2c>
  - in tx with hash: <2487522f5e2b7c48731f0951fb16aa64d47b4bcbfa5cb684ca1888416aeffda9>
  - this tx pub key: <50051729950dd8adeab440f1f6fae9aca5910b84d2583e3a3606bde83826bac5>
  - out_i: 001, g_idx: 85880, xmr: 0.090000

 - mixin no: 5, block height: 323180, timestamp: 2014-11-27 20:30:43, ours: false
  - output's pubkey: <ac74e73076cf52b403c63e1ac7e7b37e99f05c5abb857e1bb63cf6c800b90e6e>
  - in tx with hash: <9c9a7fd03ebfecf320dba1d2ecc242cea9cc97072e1c901c04db3e134405093c>
  - this tx pub key: <a35ac2cf072d7c70537c193f1b3cc1cb5d2a04c4381c965b52e9a68d9cf14789>
  - out_i: 000, g_idx: 124301, xmr: 0.090000

 - mixin no: 6, block height: 439634, timestamp: 2015-02-17 12:47:48, ours: false
  - output's pubkey: <cc4eafdc5994678ed47372e149daabbd5cb320da903601643e352acdb2c01c98>
  - in tx with hash: <d6e3d323e0a89a02f12fbaabc7110e65bfbeeb1d275c26efd0af04a243a7430a>
  - this tx pub key: <2443506852b2d9247018b2903e6b8b2ce45307b061d1054d0f417d27f4d95361>
  - out_i: 001, g_idx: 137304, xmr: 0.090000

 - mixin no: 7, block height: 458428, timestamp: 2015-03-02 14:03:47, ours: false
  - output's pubkey: <89885ee5135e4bd58fa40e7cf9b4e1fb08c7b4960cfee5bf0c267d6fee5f5d53>
  - in tx with hash: <74598d7806623848dc338fe1e34da559fa247f8cfe65bf9a1a8d5a4f806436d7>
  - this tx pub key: <0104fb0fdf9e877ba92ddf0ad5df75818f2b5ea5a36b36af7f20a1fddd35a862>
  - out_i: 001, g_idx: 140280, xmr: 0.090000

 - mixin no: 8, block height: 671125, timestamp: 2015-07-29 06:02:09, ours: false
  - output's pubkey: <e5a2e4d67eb525f45abd9782344791ace0ab625e7291350f19a26e4d871ad199>
  - in tx with hash: <cb88a4949ac48039bd7cff57e7ff02b646126cc68e3cf6a5eeb7a692700d270b>
  - this tx pub key: <134addb84028267ac2a52102a20662eca48c35c500ff4cce9fcaa70d1e55cbd1>
  - out_i: 002, g_idx: 174469, xmr: 0.090000

 - mixin no: 9, block height: 692187, timestamp: 2015-08-12 23:45:57, ours: false
  - output's pubkey: <54d719a2ba29dd7cf4cc660cfd95a6ec9a5e14380660ec62759903ba215b591f>
  - in tx with hash: <82d99aaf85310888639fa1b4e37c2af45e6d39346894e827729f0f920488bfb6>
  - this tx pub key: <0bb0c42050aec26b04d86281313888e1393e2661828bad95803774c870e4eba9>
  - out_i: 000, g_idx: 177499, xmr: 0.090000

 - mixin no: 10, block height: 717313, timestamp: 2015-08-30 15:36:54, ours: false
  - output's pubkey: <0a2ce9958d2ff7bc88b69624170689b0132d0aa7e94e1fa8d001faa79ea33f5d>
  - in tx with hash: <817e4abfdd27f6c86a24c3468e4121bd3b68bb784359c3fe3bf1aca11cf24dca>
  - this tx pub key: <f4ac7f15789304bbd8562a2c5e7a903988b932c92c0e3be0f4942a5b987aa0b6>
  - out_i: 001, g_idx: 180914, xmr: 0.090000

 - mixin no: 11, block height: 866031, timestamp: 2015-12-13 07:46:08, ours: true
  - output's pubkey: <7d1b2095560250279c6ea98a3387d03a37959076faabab1888e8ee718843372a>
  - in tx with hash: <2e89369de040326277d7114fc8c469e49a0d4155b757b7e0d085ccc112e3b8e6>
  - this tx pub key: <188918f1c183eebe1036b0298e8ab6d1d6d41eef953c21b89f3316e2e6767e8c>
  - out_i: 000, g_idx: 202983, xmr: 0.090000

Input's Key image: <bba12210a76aad9bbe74a2f0b82e014cacce9dd6e7f51eba14a087f4a582154e>, xmr: 0.090000

 - mixin no: 1, block height: 81319, timestamp: 2014-06-12 10:23:02, ours: false
  - output's pubkey: <59d275905541ff32ce429f6f0a3cbeaffd98610b8f13a03d2bf83b5427846822>
  - in tx with hash: <5a89a12e39d46de535c874726ca7f44d1773bc9cd77f9f235f40fb3d8984ab9c>
  - this tx pub key: <3ba8f0ec89369a5bb141ee3d82e607138b011faa1fdceb3c11df7e5c6945ae0b>
  - out_i: 002, g_idx: 35516, xmr: 0.090000

 - mixin no: 2, block height: 117851, timestamp: 2014-07-07 13:19:35, ours: false
  - output's pubkey: <1be10b38e02701e283aa42eaf3ae0c15174161760edf34ebea66aa6301366a21>
  - in tx with hash: <1984f68ad53a7c7574256871c5166a3180fa2e8eafd9922c1faf743d938ed565>
  - this tx pub key: <029c8417c9ebc7aa555e1fa99f7d723b73510f221d09f8a69bb72c57eba3f849>
  - out_i: 000, g_idx: 75590, xmr: 0.090000

 - mixin no: 3, block height: 131549, timestamp: 2014-07-17 02:26:09, ours: false
  - output's pubkey: <5e45548f791bbda305228ced56d94e3334185b9605ad983aa5f409775c6398e9>
  - in tx with hash: <e4483480039cd4b4e548aeb881746633e07bef3395b11e2040faff3e9522a47a>
  - this tx pub key: <e75bc6a91b28f3cf17d464e1eda75d62df4506e79c97a739db75743e67d857ae>
  - out_i: 001, g_idx: 82023, xmr: 0.090000

 - mixin no: 4, block height: 131719, timestamp: 2014-07-17 04:58:46, ours: false
  - output's pubkey: <807b3fb063546a72575e4775ac6a4807fc0a7fc82009f3cdf54b37ae3096fd19>
  - in tx with hash: <4e4be7b3b455ebc3740cc0df8a79822fccf4cab97a3e65a31f705742b9c32202>
  - this tx pub key: <c4ffa9a34cac3ddc2c4d4d92a73d3ce9561e1dfb43af085fa8ea2576270a5506>
  - out_i: 001, g_idx: 82100, xmr: 0.090000

 - mixin no: 5, block height: 497313, timestamp: 2015-03-29 17:57:59, ours: false
  - output's pubkey: <bfcbd7b2a12cbe0b2dc50c99b2f413c85ae5ef649d41d3e8966e29de6de81d89>
  - in tx with hash: <e8bb0c8a4acac89227cc5d7585c03f4e6619912d52998f1009bc02eb8b8a0110>
  - this tx pub key: <00bea208f97639e2a5e79aa4408de1e8b4d6c20a19ae337a9fb7e6751acd5f94>
  - out_i: 000, g_idx: 146733, xmr: 0.090000

 - mixin no: 6, block height: 554380, timestamp: 2015-05-08 15:15:44, ours: false
  - output's pubkey: <47b223c6661e5c56c60ce56519f34844a46bc25fe50bc16a3b9ecf8f29c30277>
  - in tx with hash: <9012db605c7377913692306f8cad5489072154dfc5b5909d34a54c3cc8e5bf36>
  - this tx pub key: <31564e79499b79c8ffdf2498e8be2a4be6e576631d7d6be93686ba7cd542c6f4>
  - out_i: 001, g_idx: 156585, xmr: 0.090000

 - mixin no: 7, block height: 649028, timestamp: 2015-07-13 18:50:57, ours: false
  - output's pubkey: <100c6760c1dd381be62244825b187e33317e60ca1379efe42880cbe3bab212ae>
  - in tx with hash: <032e96a23100de14a97295d118e188c09b9459e3d4a2d27a0220a4df87faebd5>
  - this tx pub key: <fc2cc37edb0efc11430b3062b386d2fd7032d9356a95ec5a55e11004dc44f871>
  - out_i: 001, g_idx: 171421, xmr: 0.090000

 - mixin no: 8, block height: 649323, timestamp: 2015-07-13 23:22:19, ours: false
  - output's pubkey: <93c9cb189ecbf5a7c28ecb573b0a5b15382292638edc04983c2db8ebaf8f014a>
  - in tx with hash: <a8b585d8370d20fa511d7e68abb6fb9b6d7d26bb38d65daf814340642943cd3a>
  - this tx pub key: <aa324f370c1c2392dc2b01ca107f7f63c5dcc48af45a559b8e788e0ab7ab4c33>
  - out_i: 001, g_idx: 171629, xmr: 0.090000

 - mixin no: 9, block height: 671555, timestamp: 2015-07-29 13:35:07, ours: false
  - output's pubkey: <d24bbbf2888e1198e8b87b92b7f9a709fdadd172fd3280e35b001c3e0f821f0f>
  - in tx with hash: <8309cd6c1ae42fd0619f0e115a213e5f94b793f9422ce1e2cb09ec064ac1a249>
  - this tx pub key: <31b1918e100f93941262d3a674d08ac944584aaa6e15b1c7f1c7856823e8b698>
  - out_i: 002, g_idx: 174768, xmr: 0.090000

 - mixin no: 10, block height: 816320, timestamp: 2015-11-08 05:06:03, ours: false
  - output's pubkey: <630d96407e16d3e62cacb708ec234725aae0d5a34b8598796aeff5a5742c81da>
  - in tx with hash: <74ffe67dc4a10229b8bbd401b37a5be55c7afd7d93386fafca288796daf3fe67>
  - this tx pub key: <0f939947cb91dcc90397c9071b027376c854d20507d1bee6dcf646aa33896fa2>
  - out_i: 001, g_idx: 195651, xmr: 0.090000

 - mixin no: 11, block height: 866082, timestamp: 2015-12-13 09:19:54, ours: true
  - output's pubkey: <312180de040928ffb6a693aad9b44b9f39084976d630263d9d3e877ebb99739b>
  - in tx with hash: <f1a313521ef12b178801579d36892910c2b77524f6e4fa923a5c23b64d86c84e>
  - this tx pub key: <4a17c998c3b31aba8cd35d2acfe20887232685bce511f0f2882e0ee932f6da75>
  - out_i: 000, g_idx: 202987, xmr: 0.090000

Input's Key image: <b6626bd99aed44cab74e1238a38f81965badc69d45843943a788a2a3bdbb7624>, xmr: 0.090000

 - mixin no: 1, block height: 126530, timestamp: 2014-07-13 15:32:01, ours: false
  - output's pubkey: <ceae84e2f940ef8bc288913a080665bdbff3936a0927ad647e58c0d07d760437>
  - in tx with hash: <b69a9e368c1b576b2a5f488ccb346b3a4fcb6e62506c51a2ed484944ca9ed0b0>
  - this tx pub key: <09a1e7b088dc0e535510875eaf9bd990f30d917af15bb90b4a116e9d0fc67727>
  - out_i: 001, g_idx: 79428, xmr: 0.090000

 - mixin no: 2, block height: 138806, timestamp: 2014-07-22 02:47:25, ours: false
  - output's pubkey: <2e3ae05456b133cfad072ae5421416c3b5ce675d74f4919b18b8bc73e0deff75>
  - in tx with hash: <553f27bb36c656862e9345b8675f74117215aba28f951ef854d5c6ef4e308da4>
  - this tx pub key: <4fb36c0c4f3171d343368097b4531c481f5fe46ba497a37d3bd9ab0c5612cf02>
  - out_i: 001, g_idx: 86086, xmr: 0.090000

 - mixin no: 3, block height: 270371, timestamp: 2014-10-21 19:57:23, ours: false
  - output's pubkey: <3216f747c1a7dbedd9dbab6475dfc761ac0483e052620657e1f179847a0ed1d3>
  - in tx with hash: <6fb83e7c4690cabdbe0e80abc600768ab5abb48c5d5b3ce6b80bf85275c30eb9>
  - this tx pub key: <ab66b1337bda61fd0225146d5a20f2589630a2f4ba9d623ec307618841cf08c9>
  - out_i: 001, g_idx: 121323, xmr: 0.090000

 - mixin no: 4, block height: 404487, timestamp: 2015-01-23 21:38:26, ours: false
  - output's pubkey: <d2ac65eeaae488badb61f5f06ab0a9520802582ef47ac26c133f3b868ea68332>
  - in tx with hash: <e0fa455e463f92f3ccd4b653608ce1fc6ff02d1d12966ffe7aefbc46069c9062>
  - this tx pub key: <3d3044da089658b01f89e645f42f1f494bdd85f39d62410dbc034620c4ed158f>
  - out_i: 001, g_idx: 133320, xmr: 0.090000

 - mixin no: 5, block height: 574578, timestamp: 2015-05-22 16:45:58, ours: false
  - output's pubkey: <30b4fb54acbb74a8852831f6d61f1037bc7a1543010cb416bab945126a718e71>
  - in tx with hash: <21a9a72f070c73920f89c46974800b1f44859d2856b271884645cd2730eac0fd>
  - this tx pub key: <c7f32c042cf1e021b90508ca361a8a453e64f2a3738e2bfa54642f49c3b46710>
  - out_i: 001, g_idx: 160011, xmr: 0.090000

 - mixin no: 6, block height: 584808, timestamp: 2015-05-29 20:50:55, ours: false
  - output's pubkey: <7a97120abebb9f6000fd9d8ed82736a7d9f2e31ca6beb62442fc462c206b32b0>
  - in tx with hash: <f85ac7f4a00d2da3a15ab0743282fbff1d31ebcc3a50dba89de2fd5fc1982b9a>
  - this tx pub key: <98a8b5b0725514d973ad346c9a6681b5464f31051cb94fd06716cf24805ea0f2>
  - out_i: 001, g_idx: 161537, xmr: 0.090000

 - mixin no: 7, block height: 607738, timestamp: 2015-06-14 21:16:25, ours: false
  - output's pubkey: <54defd7de92d109a2f1ad4952f5643ecf6a96197c48f799dd45d458acb9570f4>
  - in tx with hash: <da0d3310ddf6aa9aa0bd08e1a4cb020da296bdaeda1df44bb2b5acfe99dc6b27>
  - this tx pub key: <9bce35c3803e2e7d1f92f3f4dd920766d56500bb1c379d2d0ee31afa92eb22b1>
  - out_i: 000, g_idx: 165660, xmr: 0.090000

 - mixin no: 8, block height: 660296, timestamp: 2015-07-21 17:19:38, ours: false
  - output's pubkey: <f47bd52a74598e1f1b525066b858083a9fcbdb49920bd9953d5f7eb6df1e0361>
  - in tx with hash: <5db43f0253b4615e16e34c7ce28478fe08f4428b9831e4aad0e3461f2a3e403c>
  - this tx pub key: <283f77146209ee02ce6ba69d92b6abc6ec386352d9dbdac7f4433295c555fc95>
  - out_i: 001, g_idx: 173014, xmr: 0.090000

 - mixin no: 9, block height: 741871, timestamp: 2015-09-16 20:53:45, ours: false
  - output's pubkey: <fddbbcadbe3e35efc1c66b19443ee661a5571a3719f2f2739ee7753d30f77b91>
  - in tx with hash: <494eea1d939890aeeefb786a2fbf25f5fb8d09b04906b4ea621ce25a7cc738f2>
  - this tx pub key: <fe7c740ee2c422febd25301ed16092f3d40df4654209e3a95dd5757cb5ef81dd>
  - out_i: 001, g_idx: 184994, xmr: 0.090000

 - mixin no: 10, block height: 811250, timestamp: 2015-11-04 16:10:19, ours: false
  - output's pubkey: <544dd30469c8042170f90e5e3389f6f0bd3c029ab5a9be4e1ffc41442dfa6211>
  - in tx with hash: <eebe529de494d5238e42ebd66e9e808c60e6ee4d56d463d4dbc43bd1fe48c3dd>
  - this tx pub key: <200c620dbaeb15a247e4cd4425a3094f698e3c9db63fcbe3d98cbe70468416d7>
  - out_i: 002, g_idx: 195081, xmr: 0.090000

 - mixin no: 11, block height: 853883, timestamp: 2015-12-04 18:42:56, ours: true
  - output's pubkey: <cf563a97f33a0a9b90d417ec8010159e8a19f92aedad1979444710c3674edcd9>
  - in tx with hash: <949d87eea904d61bad69840b4f11af62bf83d23ac95e749bfc7038c70c4080b4>
  - this tx pub key: <206a89959fd695817086f73d40a398ec94d34b25666ad770047e5ffa3ea86cdb>
  - out_i: 000, g_idx: 201003, xmr: 0.090000

Input's Key image: <011e0aca3bd5c3cac039440a720b9ff70a88a653e13718b47189af43f0d02e4d>, xmr: 0.030000

 - mixin no: 1, block height: 82150, timestamp: 2014-06-12 23:01:27, ours: false
  - output's pubkey: <9c9da55ae680f2f465d419d711b97b72d7824e1ae87cb19cf0b398bf09d02061>
  - in tx with hash: <8994cb7d1231d6e7ea461ef2331165f081196234159653537941e390054447a5>
  - this tx pub key: <eb3b7cf740a60dc6c5745783984c42dfaaf19755473c00a48c6f9ce2d73a7e19>
  - out_i: 021, g_idx: 78591, xmr: 0.030000

 - mixin no: 2, block height: 82692, timestamp: 2014-06-13 08:13:06, ours: false
  - output's pubkey: <56f840dde85daeadaeabf5b312b5513d1a88f0806372d5a33e7c3dad47897ed7>
  - in tx with hash: <9df2efe941fe80cfae5afc8c929a22fe0190995b15c7d11c0c392371cc4af6b6>
  - this tx pub key: <e709a210615c6626a396abe0f8eb2cd4580a5d4069384d36d60af243d71c7530>
  - out_i: 090, g_idx: 81990, xmr: 0.030000

 - mixin no: 3, block height: 83049, timestamp: 2014-06-13 14:17:31, ours: false
  - output's pubkey: <5e4cb0a2bba2cb9249b44f3d4ed008ba7340c14bcd636084981dad0e83f0e0f1>
  - in tx with hash: <80934ac4436f63834dec7ab0478cb3600a1f1048a9e1882de7baf6389c138536>
  - this tx pub key: <bb07950ffd90d7ca50cf18570be4bf04b2d6e61310242d6ca8f5cff2280309c0>
  - out_i: 087, g_idx: 84128, xmr: 0.030000

 - mixin no: 4, block height: 142692, timestamp: 2014-07-24 18:38:44, ours: false
  - output's pubkey: <deb03161f1d3005feb2398fea3d22d65b04a784ec91861ae501d4060e675da91>
  - in tx with hash: <cd4ae91ad96fa5e0d8168dc315bec16763774334f94729e682660c6cdd2302c2>
  - this tx pub key: <c9e1080967dcbea46c87f22e6c2d3dc0793b9dfa3c6e9306cd6ab7c89a28a381>
  - out_i: 001, g_idx: 151724, xmr: 0.030000

 - mixin no: 5, block height: 199816, timestamp: 2014-09-02 13:21:15, ours: false
  - output's pubkey: <6d518ccde8be7f7798c7befb6109d45d92f6db722186142a0226bf1ad597f577>
  - in tx with hash: <03f7899232b5aedc8cc0145a0dbae276c7dfec40d416f5cae5015d464b0c1f38>
  - this tx pub key: <11df096dfa5e5f8bef61ce938e2e36cecd66d94e314811ecf9a8e13a34694f59>
  - out_i: 000, g_idx: 170944, xmr: 0.030000

 - mixin no: 6, block height: 454579, timestamp: 2015-02-27 20:38:16, ours: false
  - output's pubkey: <a178166ddbb01e8b2ced49e8d38990e22d4377a43aab6a2d66e347a18dad244f>
  - in tx with hash: <00b05f538245cf47f58cb5ef7fb0203efbaa4def901c44a60b1be347aa407a88>
  - this tx pub key: <46039cba072a0f1ed8eaaf6a2a79c9d2691704cb801a7eccec8160a6fc309577>
  - out_i: 001, g_idx: 185486, xmr: 0.030000

 - mixin no: 7, block height: 591026, timestamp: 2015-06-03 04:52:50, ours: false
  - output's pubkey: <732e3100bce01897a69bbf5ff4433311924dcb782f45874f649aae4576e8ea84>
  - in tx with hash: <6d339a900a19053c82a2fc93d5f3abaca04cc51a86f3107d2ef0aad614c06a8c>
  - this tx pub key: <3d837ce543553732f4e660824d64eef0fab422c4fbba97acec69b5de0396d636>
  - out_i: 001, g_idx: 202881, xmr: 0.030000

 - mixin no: 8, block height: 591696, timestamp: 2015-06-03 17:16:01, ours: false
  - output's pubkey: <59d666ae806dc00b99813f3e0e4a4edadc3fc02db6374d7a14b010982f0832b6>
  - in tx with hash: <1d6081595001c78c30f07f31a8ca71c24435d2789018f8a2ef56e6168e82b215>
  - this tx pub key: <315115f0e8d5ab34f92ff4374336327166f789bd066c8bbab260c1482a010c85>
  - out_i: 001, g_idx: 203405, xmr: 0.030000

 - mixin no: 9, block height: 612212, timestamp: 2015-06-18 00:17:22, ours: false
  - output's pubkey: <f922b611a1d9ec488a4ff091270034fd7f31b4bb3b08732d0027a45e22ba45e1>
  - in tx with hash: <09e353f4d43f58e7508d0e8a5bb6541c482e5e70e3ddc8466e11e5a47855da76>
  - this tx pub key: <b65d5746cad778afa6e411d772752772fd319ae7e35e867ad62e2b2410dfb00d>
  - out_i: 001, g_idx: 205723, xmr: 0.030000

 - mixin no: 10, block height: 688654, timestamp: 2015-08-10 13:45:23, ours: false
  - output's pubkey: <1b42ee4a844dc99614904fc229cffd83b95009ae869fc0a96481f637740a784d>
  - in tx with hash: <ee2ac8f6c286c0b902f67533739259ae13a748ee742bf5aa8cb5899a33d5a8ec>
  - this tx pub key: <60a1965ce0e0e4e62efee9599ec041f5566e603d192d7b57c529df70a0e50291>
  - out_i: 001, g_idx: 215018, xmr: 0.030000

 - mixin no: 11, block height: 866025, timestamp: 2015-12-13 07:42:49, ours: true
  - output's pubkey: <9adbf80f1eb447addbd50c65237b2b8d122effa84a780dc20e3606b05679ccad>
  - in tx with hash: <861f46ad74d9743392e15b021f83b1594c115b6cc78a9f9c6866642eb3a3cd14>
  - this tx pub key: <bdd7577c9fda18a3363c9c6bedf0a89543fb3861a539f1535489f669888be379>
  - out_i: 000, g_idx: 238496, xmr: 0.030000

Input's Key image: <c5701f3841c20c57ac3bd44eaca5440acbca310e8463d50fc218efec23c74803>, xmr: 0.700000

 - mixin no: 1, block height: 190570, timestamp: 2014-08-27 01:17:20, ours: false
  - output's pubkey: <b58935891e6433a7fb196541d622008c9409b928f2573801ebd413ddf011c725>
  - in tx with hash: <8d5e11711c4070252fb5c93e5163808fe6177ba53aac7140b8b3fb4fad62c1e6>
  - this tx pub key: <59320a5542f736b57a8a1288a3f8a5784ed4a011ccf697bc6041745b15be52d7>
  - out_i: 001, g_idx: 126194, xmr: 0.700000

 - mixin no: 2, block height: 363941, timestamp: 2014-12-26 12:18:40, ours: false
  - output's pubkey: <441c03cdbd3a6f0cee08336b3807beffeabefc27f41bdb9e049ecc3594816e49>
  - in tx with hash: <5a15505b54592bc815b43c34a269bb278224a7d40a21f78e7523cfc3f9026288>
  - this tx pub key: <b2b0543b39b55305b58b72f510133c2f9479f6c003ee835dd12b92bdfe3103cd>
  - out_i: 061, g_idx: 235401, xmr: 0.700000

 - mixin no: 3, block height: 381262, timestamp: 2015-01-07 16:22:09, ours: false
  - output's pubkey: <e3fc7a24fb298c73922a3529594fce8a79803ad353ef48742092301375606a39>
  - in tx with hash: <72de157e2078daa58a3513f887fa40557dc66c2dd19839bc0b472b63699234d5>
  - this tx pub key: <e32092786b4c27583f5506d5c0f7ac4715d13b7209984e113af773090d6b1d51>
  - out_i: 035, g_idx: 240622, xmr: 0.700000

 - mixin no: 4, block height: 382886, timestamp: 2015-01-08 19:06:47, ours: false
  - output's pubkey: <4c5b890a15e94624a876e2b4175b635e42638a688b333411f8942bde94fb6b44>
  - in tx with hash: <4b1506b5e4cee6af4de0142c446d200fbbae39dfac072cab6b8fcd0890223569>
  - this tx pub key: <b6d9f95cc0f118475f99c3227bd33d6b5a6847c8d2e79ddf1329ffc5de69ee7c>
  - out_i: 044, g_idx: 241164, xmr: 0.700000

 - mixin no: 5, block height: 528324, timestamp: 2015-04-20 09:49:46, ours: false
  - output's pubkey: <c8a0ca6495abc1d5666ab05b5698675a9c7abb4f09221889ddc3c75b030fe531>
  - in tx with hash: <c22d4af735a58197c0d8f3223f3e0e1c0fccc05bd001085502840dbb69942041>
  - this tx pub key: <bccb1e9acefe61194664788775ef155204f95e910d29d576ae81f5ef3b466d9b>
  - out_i: 003, g_idx: 301166, xmr: 0.700000

 - mixin no: 6, block height: 603863, timestamp: 2015-06-12 05:00:32, ours: false
  - output's pubkey: <5fd81198ccb85a24e57791f46b2d61897df4c42c0a0d54174008cfc6580fa9f7>
  - in tx with hash: <55b26f91fae45fd83da9146c6851d8406c15ddb8efbc6f0e26a4251ff1a3f076>
  - this tx pub key: <5762f69361d92b5bbd7d7d3cdce1fe3a0893146af21af8d5a435414f341e3830>
  - out_i: 039, g_idx: 318758, xmr: 0.700000

 - mixin no: 7, block height: 619983, timestamp: 2015-06-23 12:04:53, ours: false
  - output's pubkey: <60197a0c4313a27a435b6958d85f4bdb28cf788fff58ae2cbbf5ddad97317442>
  - in tx with hash: <06d8334469a20add3e3674bf4d5ddcb806543a4d0732636847514d9972328626>
  - this tx pub key: <083076eea82fd9ee23c2b8a5397661080c57cd25469d4dfc02d92f30aeda8c18>
  - out_i: 002, g_idx: 325229, xmr: 0.700000

 - mixin no: 8, block height: 626000, timestamp: 2015-06-27 16:13:53, ours: false
  - output's pubkey: <ca8d7b00c9226cf42eddda4e1c292eab3fd45f96f359e0bc4402c31908138f5a>
  - in tx with hash: <facb9ee636dcf346370dde5cb46138ac669e1b98dd2a250a7682cddc0d4447e1>
  - this tx pub key: <c788a5c41b82d882d61ab2c3b95b22d0e2791c8abaa651b3f9c1529b9ed0dac4>
  - out_i: 053, g_idx: 332211, xmr: 0.700000

 - mixin no: 9, block height: 630686, timestamp: 2015-06-30 22:28:18, ours: false
  - output's pubkey: <86fe488213ccb26fcd1543f79fce965ba3255de973be723dba26df8e88f4a4d7>
  - in tx with hash: <b9258b1cc738ee6b873bad0e488f9986fe7c89e490a40a6c9fe8f98dd9129147>
  - this tx pub key: <7c08149ab8c9468ca7c985523114a8754b48b50f1094af9f182783851689038b>
  - out_i: 016, g_idx: 334669, xmr: 0.700000

 - mixin no: 10, block height: 730332, timestamp: 2015-09-08 18:26:55, ours: false
  - output's pubkey: <6532a9145828cd0cde0165c9962b2977dbbf57a973da869db1d7c82eb145f3e9>
  - in tx with hash: <14a535fdd9fcfcd89ae70f4c8ca75f593e46efe6fea5c82f8d7451028f2dcfb8>
  - this tx pub key: <2f80cc31466a2bd4f8fffbd06ab24cba747dcdb02eff0ce78fe2e51e82eedc5d>
  - out_i: 003, g_idx: 354108, xmr: 0.700000

 - mixin no: 11, block height: 853883, timestamp: 2015-12-04 18:42:56, ours: true
  - output's pubkey: <7be3aa80f8f452a26518ab57b5efe4c4a749008f123648dade8169e861df80a1>
  - in tx with hash: <949d87eea904d61bad69840b4f11af62bf83d23ac95e749bfc7038c70c4080b4>
  - this tx pub key: <206a89959fd695817086f73d40a398ec94d34b25666ad770047e5ffa3ea86cdb>
  - out_i: 002, g_idx: 384417, xmr: 0.700000

Input's Key image: <4462bd9dff6684a44290af0fbc6e4a1ebb1d7c46161780de47dfb37a3cf06d81>, xmr: 0.500000

 - mixin no: 1, block height: 264098, timestamp: 2014-10-17 12:03:30, ours: false
  - output's pubkey: <404ee376d2ce169664ccf55ec3c8cb6a1904eb5fa1d5c728bf0e46bb66e905f7>
  - in tx with hash: <534e69b3a8c1f4d7fe16d5238e5a7f664e3d6b429ffcc475bc6b30bedbfae88e>
  - this tx pub key: <3ac53d770d14ecc02ad6e41434f59f82bfcf7645a50db29bbcaeaed6ad2cf312>
  - out_i: 023, g_idx: 384885, xmr: 0.500000

 - mixin no: 2, block height: 282894, timestamp: 2014-10-30 16:01:07, ours: false
  - output's pubkey: <58d4035c964c6a8bed65a5e9ba7fe9ca6e1059eb3893e63b944a10c553803028>
  - in tx with hash: <09bb0bced0c060c227a9ccae4e8064e2297cb4ad73dbff15e6be43c6b525c87c>
  - this tx pub key: <15a3937cb141c2fcab1a60b03b9e6ea56417c989ef849112670188270b3d9b72>
  - out_i: 016, g_idx: 419198, xmr: 0.500000

 - mixin no: 3, block height: 302335, timestamp: 2014-11-13 07:31:00, ours: false
  - output's pubkey: <c3838ea2026aa91c03bda2cd0e8cf07a6220c856099507bfc68878277ac9dc9c>
  - in tx with hash: <4a481756f707bc6b1471f5749b2018032233cfb5448da9cb4e2d7e697dd6977c>
  - this tx pub key: <aa168ea710dde16a85dcca604f85d1554c0afbd277f61f25a5a11eeabf78f6ea>
  - out_i: 046, g_idx: 452033, xmr: 0.500000

 - mixin no: 4, block height: 316984, timestamp: 2014-11-23 13:32:02, ours: false
  - output's pubkey: <c7a6e678e82773eda41871c6cd89538c20e2bdb5ddaf1b83af7531925869a524>
  - in tx with hash: <3964d8d41f24fce5eb707940082737bddfdb31726c97f716efb4b819eedf21dd>
  - this tx pub key: <f6cce33f0436be3da446a75d315a9c50c5739f77f81186743a611f5f9c8f9a49>
  - out_i: 004, g_idx: 475587, xmr: 0.500000

 - mixin no: 5, block height: 409013, timestamp: 2015-01-27 01:23:36, ours: false
  - output's pubkey: <e247516db6ad43cc74c0c67b82329c95806860b8b199e8b4cd462efc43f3dbb1>
  - in tx with hash: <f52112d8a16bdce2cc224bb467598b5af01f1127b3854f85b878c22f969e9794>
  - this tx pub key: <3f602dba3c42bc527cc742eb41bb2926442de89939178d6acc9d3f7b8c63b763>
  - out_i: 019, g_idx: 610797, xmr: 0.500000

 - mixin no: 6, block height: 494565, timestamp: 2015-03-27 19:35:10, ours: false
  - output's pubkey: <471c557fcf5c980925975e8612bd2d7ebed8f5de130ddb683f8548223a009f0e>
  - in tx with hash: <6857904f4a5eb8c4fe90873429a20c8fe9fb6ee3b12a444e77b8d022c729b3b1>
  - this tx pub key: <019b4d37a9207da76a735d6396f8743368866295a159c717d3e43cdce95faec9>
  - out_i: 020, g_idx: 706578, xmr: 0.500000

 - mixin no: 7, block height: 564005, timestamp: 2015-05-15 08:25:37, ours: false
  - output's pubkey: <3c7287185f6522364469a1defdf27f9aa03a688020934e0127c8ddccaab50f16>
  - in tx with hash: <d0d461861ecb42d4ce8dd579cfcc0677dbb5a57be3e19a09e57339c6af2498a6>
  - this tx pub key: <d58807b094ac2ae15d23fcea3692f21ed3019c25f70215993efb996f44953b64>
  - out_i: 014, g_idx: 755967, xmr: 0.500000

 - mixin no: 8, block height: 589350, timestamp: 2015-06-02 01:01:36, ours: false
  - output's pubkey: <d98696a0aaefc46b9e60c121b4099c9b92b384b5deeb106c722efe6a8bae3c74>
  - in tx with hash: <e19e5c7767ccd18359fd1018723ceaa0d0b787ed9be3f967c8d6f99558dd6609>
  - this tx pub key: <e4ab11485e28f46aca4580d8497ed5e8a9ae8b1250eea47195048725de431ed2>
  - out_i: 041, g_idx: 767934, xmr: 0.500000

 - mixin no: 9, block height: 678911, timestamp: 2015-08-03 16:28:48, ours: false
  - output's pubkey: <786f6dfca3f62d3625febcbcb7f2bb844d88d77ba19c3fe7283b953be8b9d3fa>
  - in tx with hash: <e178dd410b8fafc99745e28a645e61ed1203610882632d144e42e9f37e2e0569>
  - this tx pub key: <ffc399d88522bac4c181636823e63c8fb76014a1b4e2f3a6745bdcaccf4958ef>
  - out_i: 019, g_idx: 818233, xmr: 0.500000

 - mixin no: 10, block height: 866026, timestamp: 2015-12-13 07:44:26, ours: true
  - output's pubkey: <a7663b292d1de921147e1913da3f35d7953e941fb83d985ad9f6a245ae5427bb>
  - in tx with hash: <1a948a528f1ded263ad65446ed39626cde07cff3a37fe12878ba86671e01ec7d>
  - this tx pub key: <4b042442cf9356cb68f9c6b6edd528e6113d93742263e469c66ee15bfc19d6ce>
  - out_i: 002, g_idx: 898915, xmr: 0.500000

 - mixin no: 11, block height: 880143, timestamp: 2015-12-23 08:35:07, ours: false
  - output's pubkey: <7dfe08e94ef45fb6a109c89ce4d269b24f29a78bcd4dfe860f8e06d391e2ea37>
  - in tx with hash: <bf67ac1543b46bdff144e0ed315d5abac9f01b5b1892238278d4abbe41dbfe4c>
  - this tx pub key: <be08cc91c03aa5d757f5f7aae100ca310161afee3ef4086f7a83d3339f7e5bd0>
  - out_i: 046, g_idx: 904569, xmr: 0.500000

Input's Key image: <a0cdd72f36e8d572a22333cacf7accc95f7e2b4c80cebd807ec1c1f51bf95e67>, xmr: 0.600000

 - mixin no: 1, block height: 173566, timestamp: 2014-08-15 07:00:26, ours: false
  - output's pubkey: <877958f5eb650130b1ffb2efbc1c6a8edcf29f215fdf70c66544bcca3028304f>
  - in tx with hash: <7019f253300af4edb9328ac55fc3bd18fa80450d9a4e6652a581b23b68c4c2d7>
  - this tx pub key: <f0bfaabcdd6ac0e1bcc5f862b05330b31106c1a6ee88dd36d1b85d6fdeca0a62>
  - out_i: 029, g_idx: 149501, xmr: 0.600000

 - mixin no: 2, block height: 177859, timestamp: 2014-08-18 07:02:41, ours: false
  - output's pubkey: <569c742582fed356313ae699d4a8ccb65326a7cb686afc72e346215f9650b9d1>
  - in tx with hash: <21ab2951ee7846b4e8b7b6de72b4152db86da8519a1087488ef429087af89931>
  - this tx pub key: <a869666fc197c29ed394f6f32f89bdd6d8d65aad6ba05ff5050b0b4052fcc0b4>
  - out_i: 034, g_idx: 153981, xmr: 0.600000

 - mixin no: 3, block height: 248793, timestamp: 2014-10-06 17:19:01, ours: false
  - output's pubkey: <0ae6a9dff09f76508c50651b4593c5515098b2b3494700118f48438f32f56178>
  - in tx with hash: <3e83e1423f9fcaccf16fca75ab9888c62159052c750c3ade17b4415be7993b77>
  - this tx pub key: <411e9e10d6caaf649c5965c13a8c353745f06d7d765b473dd3bc586215560e8b>
  - out_i: 043, g_idx: 228038, xmr: 0.600000

 - mixin no: 4, block height: 258694, timestamp: 2014-10-13 15:58:53, ours: false
  - output's pubkey: <03e4610606b02db1b55622ad9a2d175bdddbb966637eae35e6a1a4a8781ee9ed>
  - in tx with hash: <fef78888c2b1ae1d7c3261e472a9d4e7004974fda312417e6167e708d4b59504>
  - this tx pub key: <900e90dc0d032d747a1a614af6d3d94635da6727f9db55666191091532746621>
  - out_i: 001, g_idx: 235351, xmr: 0.600000

 - mixin no: 5, block height: 321190, timestamp: 2014-11-26 12:58:47, ours: false
  - output's pubkey: <56f3dffa23ada5c4ab063a7072b2edad33f335c98b7cf3ab757278277e0e18d9>
  - in tx with hash: <1e4e997340ae547a35beee27a7f9d92da8951b4e067c475085f55db099e72500>
  - this tx pub key: <076f0b400beeccd57d62e793e6048d218427100dda8bfa09668688f959248951>
  - out_i: 022, g_idx: 288268, xmr: 0.600000

 - mixin no: 6, block height: 346754, timestamp: 2014-12-14 10:46:45, ours: false
  - output's pubkey: <e84e15d7e70672c6581eaea0435e99160b850a053b95bc5831ca1eb04613b667>
  - in tx with hash: <5162a8869efbe8a6f42ada0c3173b243f87e124682802394524ef3a799ca7215>
  - this tx pub key: <46cea8bc31e4ff6eb26c01a99aa65951d2339c5fe21d76cd76362b537d7eec74>
  - out_i: 037, g_idx: 307496, xmr: 0.600000

 - mixin no: 7, block height: 440212, timestamp: 2015-02-17 20:58:43, ours: false
  - output's pubkey: <f4ee069cfb04328542bf04f871f81839b4d66080fd5953579ae877eb05c85841>
  - in tx with hash: <61d9174f12e4aa66fb72e220bfcf8642f427515ccd053181e23391db18f46b21>
  - this tx pub key: <596ee3b40f54a6dd175ee50b72fa3e2c2e465f3a5f1382e76ae17e2f7a4ad041>
  - out_i: 006, g_idx: 374375, xmr: 0.600000

 - mixin no: 8, block height: 493692, timestamp: 2015-03-27 04:42:02, ours: false
  - output's pubkey: <b4a13772d94bf8718caed0d9261cf1d978cf75f7d2c202eaa4150e42c52aab69>
  - in tx with hash: <06ffa1fe135c5edef99b3ef6025ee712bb582b58486db43e0311a75064cbf0b1>
  - this tx pub key: <289f2746abf4bb7d97fb9bf1e80740d4d0546d885cdfe3ba577602149cb44fa7>
  - out_i: 000, g_idx: 395456, xmr: 0.600000

 - mixin no: 9, block height: 524611, timestamp: 2015-04-17 18:15:01, ours: false
  - output's pubkey: <77feeb42d813853b28db0f8eaddcabbe881e4b41681eb2a7a291a09777573a77>
  - in tx with hash: <ad1e65757c8cc985ebc7409a1cdd0fdcf3315e2c6fbe49be8356545bccf1994e>
  - this tx pub key: <4b495e5846018ad1b4f6b3bbfb44aee82f6450c5b7c8cad3b3254fd8814c2d1b>
  - out_i: 002, g_idx: 406712, xmr: 0.600000

 - mixin no: 10, block height: 633661, timestamp: 2015-07-03 01:16:28, ours: false
  - output's pubkey: <d7a72bbf931e9da6c3ddd4deaab17e7357f5a53817116e1618738dbd8cba8ade>
  - in tx with hash: <f153e9865b69b28d83ad9db9df6f60d0eb14b4e44dbca574b520de9a49710a6f>
  - this tx pub key: <01a1593dffd6d38bfb397bf284bb9b8b71715e8643a7ff3a5f36620b42a06600>
  - out_i: 002, g_idx: 452067, xmr: 0.600000

 - mixin no: 11, block height: 866031, timestamp: 2015-12-13 07:46:08, ours: true
  - output's pubkey: <d9aa437329d0f1073feb93c2bbf2b0a60a94d2a8f50d195ec1a27c01f972316f>
  - in tx with hash: <2e89369de040326277d7114fc8c469e49a0d4155b757b7e0d085ccc112e3b8e6>
  - this tx pub key: <188918f1c183eebe1036b0298e8ab6d1d6d41eef953c21b89f3316e2e6767e8c>
  - out_i: 002, g_idx: 517178, xmr: 0.600000
```

## Example input and output 6
Transaction hash given along with the corresponding address and view key.
[MyMonero.com](https://mymonero.com) was this time.

```bash
./showmixins -a 45aVogkbq3fSVvY1p8BWzA9r1HjBi26u57DrJ6iNrG7AG2r6zqpyzFqikuT863a8A8PMn2Akb21yrdw1VcYL4qJc888Np7H -v 1cdd587034c25fc3de4d09be5d856464a2484251503aa009779393a3137e5d06  -t 9040535c2b7527ca7511a9b482a5c0d3fa25a1a135a7b791bc2bd6e6ec004384
```

```bash
tx hash          : <9040535c2b7527ca7511a9b482a5c0d3fa25a1a135a7b791bc2bd6e6ec004384>

private view key : <1cdd587034c25fc3de4d09be5d856464a2484251503aa009779393a3137e5d06>
address          : <45aVogkbq3fSVvY1p8BWzA9r1HjBi26u57DrJ6iNrG7AG2r6zqpyzFqikuT863a8A8PMn2Akb21yrdw1VcYL4qJc888Np7H>


Input's Key image: <fcd88a1a842d43cc6e787a1cb014473270226d71b37b3c631521224dfc5bc1b3>, xmr: 1.000000

 - mixin no: 1, block height: 123158, timestamp: 2014-07-11 06:31:03, ours: false
  - output's pubkey: <d6b91dc24c4ef4225f83a445f4b2859fc20556c1cd6ee102fef83d1a4a9f73de>
  - in tx with hash: <8ab059b21793dd20a5381bcd4acde8b20622126e7a7d43acdaf9edf8b40c744d>
  - this tx pub key: <62e9199dca865493a69580f3c3f911396d9e04160d7d4451ab47393faf54f90e>
  - out_i: 047, g_idx: 91604, xmr: 1.000000

 - mixin no: 2, block height: 124518, timestamp: 2014-07-12 05:05:04, ours: false
  - output's pubkey: <72aa5d0082f607802807976654d2bf9cec5a394b98bfef216ee64cef7a609d7b>
  - in tx with hash: <a0632b39a6bf5d2856e7746f951b87d8aa657af4cb7cf55eeb59baec0de9d211>
  - this tx pub key: <432122863e489536f32e7bb275faf953543e7b0b688b99a4b4c6c7038b7de2d4>
  - out_i: 006, g_idx: 94285, xmr: 1.000000

 - mixin no: 3, block height: 125461, timestamp: 2014-07-12 21:04:32, ours: false
  - output's pubkey: <016d0fa602ed2cb2fe6a931480e260e327ec4f8f97c2bb5e742fd4b22d834372>
  - in tx with hash: <0b1163df3bfc8b8419a39c4768568eb7a38317be49a85cfa9878259a2c61811c>
  - this tx pub key: <83e64279f9ee73fec4debb792d284d13cbddbf9647a1448cb8fdd2a27aeadc18>
  - out_i: 024, g_idx: 96223, xmr: 1.000000

 - mixin no: 4, block height: 222646, timestamp: 2014-09-18 12:10:51, ours: false
  - output's pubkey: <d6ea13f7090efb3131b0345439ec71bef8c0904580346a12da39cdf40eca45da>
  - in tx with hash: <cc2e9ffa7b08406f47b20bcf69d42e26cb4eeb6a3580acf82c4aa9588a4b720d>
  - this tx pub key: <88dd561a11b6c94b7f0cb06924d80af3b5d77ebcba1dc35cd843a3ec8c11733f>
  - out_i: 102, g_idx: 265927, xmr: 1.000000

 - mixin no: 5, block height: 223459, timestamp: 2014-09-19 01:01:37, ours: false
  - output's pubkey: <2ad82033544840fff6c37821d8dc8d2089f0a18e332844639fd406176b2a86e3>
  - in tx with hash: <fe9affd1417dfc859f8ba49e9f195d69f9f23cb88317fe9c211dfb4551dcdb70>
  - this tx pub key: <dc4d892df7768582641e8082089318ee61d4950d855489a292ce47882f1a8a2e>
  - out_i: 054, g_idx: 266896, xmr: 1.000000

 - mixin no: 6, block height: 306611, timestamp: 2014-11-16 06:30:13, ours: false
  - output's pubkey: <aef21e03281e8e5c995276a0c45a1b23c5073942c3c6e943d6b31857300a4d8b>
  - in tx with hash: <8f17dea8c98d2430b9a0ae702920e18bc5f21a8763a893cad9d45f76d70411c5>
  - this tx pub key: <1e8ef1b9ee784008fa4897c03dc0c56fa3b94605d6245f5266b55bcbb85f64fa>
  - out_i: 056, g_idx: 337266, xmr: 1.000000

 - mixin no: 7, block height: 331494, timestamp: 2014-12-03 17:01:13, ours: false
  - output's pubkey: <a25fe80d01f78a2e551e1974a3ea9789daac0c812af6d94e20211367b3df4702>
  - in tx with hash: <e70285f78c5213bc42f7206204c2dfadeda6ae87ab80d4635598b6a285838514>
  - this tx pub key: <3c287534e5248969777c7f55eca5e2e8a265a5092dee8b97118b35a87a445674>
  - out_i: 050, g_idx: 355827, xmr: 1.000000

 - mixin no: 8, block height: 405972, timestamp: 2015-01-24 22:18:20, ours: false
  - output's pubkey: <5edb09ea34f4eca7d32bf2e5a2d86bd5816784cdc3ce99be3ae1b131f8c63cad>
  - in tx with hash: <79a22fea87b26ba1e48ae0692784c99485d204352babd82a0be3a7257457d19d>
  - this tx pub key: <9c51c755cdeb00297167ae5565ff0cda8bf9f17bdc2388d6fd970cefaac726b2>
  - out_i: 003, g_idx: 394726, xmr: 1.000000

 - mixin no: 9, block height: 439421, timestamp: 2015-02-17 08:44:37, ours: false
  - output's pubkey: <bd4ed61db66405f48c418fbae3db4f6d51c230a3e81ccee378b4defef7f65490>
  - in tx with hash: <00d3ba5a8b68f93182603ba0459307e42861835b14ab2e740b610dcfbdb8a10e>
  - this tx pub key: <a097c65ee7e09d1942e55e6743d3cb3586130ce67dbe2b4a70441d38e03e4287>
  - out_i: 003, g_idx: 441108, xmr: 1.000000

 - mixin no: 10, block height: 488609, timestamp: 2015-03-23 15:55:21, ours: false
  - output's pubkey: <3535cef80eae8378602cdc1023810b72a6f732d4a92ca22fba7198eb72563e60>
  - in tx with hash: <df22d03e144bcf6dac519fcaf4ebdc9d302ceb969fba5fffe5af5049bf6f18df>
  - this tx pub key: <217ecee91c0b2b44a57dfb3001f53cbe7b873d72c8f388ea6266a63df63200a8>
  - out_i: 002, g_idx: 512487, xmr: 1.000000

 - mixin no: 11, block height: 920251, timestamp: 2016-01-20 17:32:47, ours: true
  - output's pubkey: <0247cb9d7a2886632f4ae57295e6ad1f3eb0455cb2735896f18c4f11cd57e9e7>
  - in tx with hash: <d503cf26b56d1797f814fffd165da735bfbd94961c1e0399fe46600e67e9c739>
  - this tx pub key: <5654bb04d0ed5fd7d8d04292aa5b5ddc84263fcfdccfa39942c6f87266fd1126>
  - out_i: 002, g_idx: 673104, xmr: 1.000000
```

## Interesting observation

The interesting thing about the examples provided (and many more I've checked)
is that the real mixin in each input used in a transaction is always last.

And this happens because I'm relatively new user of Monero. My wallet and xmr are
 not that old. Thus, because mixins used in each transaction are always sorted
 from the oldest to the newest, my real mixing is always last. 





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
