# Show mixins in each transaction

Mixins represent one of the main
advantages of [Monero](https://getmonero.org/) over other cryptocurrencies.
[http://moneroblocks.eu/](http://moneroblocks.eu/) shows the mixins used
for each transaction, but it lacks the ability to identify the real mixin
based on the address and viewkey provided, and also it provides very limited information about
the mixins.

In this example, those limitations are address. Specifically, a C++ program called
`showmixins` is developed. The program prints out all mixins used in a given transaction. In addition,
if monero address and privet view key are provided,
the true mixins will be identified.

## Prerequisite

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

## Example input and output 1
Just a transaction hash given, without corresponding address and view key.

```bash
./showmixins 49503c381ed74da2079697f0e8b7228608da3cade22575774ab8cf5ca425c3fe
```
Output:
```bash
tx hash          : <49503c381ed74da2079697f0e8b7228608da3cade22575774ab8cf5ca425c3fe>

Input's Key image: <54802347b456a6dd632aea85cf970b09244107b6d5cea924feb7deafdc37cf9d>, xmr: 1.000000

 - mixin no: 1, block height: 67326
  - output's pubkey: <9f3145e43d7e0e3bbeb57d5a2fafef952d315bac341e507645621ed86efd1155>
  - in tx with hash: <21885df01a25c548ddc0bb26dacba7fcc63f8c2810e193d4048fccb9791b1b38>
  - this tx pub key: <6870bc701bea76bd66174216450eaae37166b080d1e7c3db1ffb3d760316f98c>
  - out_i: 175, g_idx: 8381, xmr: 1.000000

 - mixin no: 2, block height: 143814
  - output's pubkey: <eeaa6f0a7cc1e975815743f12434b81a921f96c650fe269159dae20ef4077061>
  - in tx with hash: <56aabcd79cd2c063bd40636a2ca69e9933c95e93ec3ae15e93beafbae0293a83>
  - this tx pub key: <ded232fda057e79ecc6353a6a2fd34a3cd0a22053123aa6e35f839f8715b2d99>
  - out_i: 039, g_idx: 132680, xmr: 1.000000

 - mixin no: 3, block height: 153000
  - output's pubkey: <ad89c7b09b1f02ad872270510ccb96d24252170cd01bbbb45b307daf89e7ee5c>
  - in tx with hash: <0daef7d911fc62ae50ee0134cb247fcf97061091fcbb1fcf4d96d1c9cdb8a969>
  - this tx pub key: <d841906d5a04d72d4585bd37c23f3501f645f407a392c86d1aaf00ee25842c86>
  - out_i: 036, g_idx: 153950, xmr: 1.000000

 - mixin no: 4, block height: 168055
  - output's pubkey: <623eba3d75cc706f34b62c50cc212267e86f50df123741f1deea039da04b6a4e>
  - in tx with hash: <31aa16467530036597f50f07fc30c3c8f8a8df55b19de831fcf3d2c18a951e1f>
  - this tx pub key: <144ce215672c8067b8dc172c2b04ac3e11dfc5fcc027c8ed19b327550dfce532>
  - out_i: 039, g_idx: 180712, xmr: 1.000000

 - mixin no: 5, block height: 277037
  - output's pubkey: <f8f1ac2151024887959891d6021d84e6289a02b8743b84f941df15e387fc7c95>
  - in tx with hash: <fc27fd68ca723784c7b9e0459cc9a53779bd1bd317f5a255114d897ecbed7464>
  - this tx pub key: <d535a1fa0eb144794759b6e3adbc6389fce8e04bf172dc08c9dcc2325cb44975>
  - out_i: 202, g_idx: 316577, xmr: 1.000000

 - mixin no: 6, block height: 539756
  - output's pubkey: <b81195393cc497f4deff5dbd9f6fc04d4c492e6c523aadb40dbd29cdf38d4662>
  - in tx with hash: <6143d5d3f5ada27c1eddc46176607b67f492b1e9427757174e3e3215eeadaf63>
  - this tx pub key: <b458cc6c05f38339a33036c3ed992997a7511b6148256c6ddf2d6742e9f4c282>
  - out_i: 049, g_idx: 547897, xmr: 1.000000

 - mixin no: 7, block height: 844912
  - output's pubkey: <852e2f8c919988294a15a65a4be8adff70e14c5907b0fee7e2ee005c134345e4>
  - in tx with hash: <27003d209ae8854a72735e0cb14f46879dafbac65cf593fb880926a2a674efce>
  - this tx pub key: <70fca5b6be58dbe39ed3fc229bb2a11ccceea89073591a18749c82f816182b37>
  - out_i: 002, g_idx: 654842, xmr: 1.000000
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


Input's Key image: <54802347b456a6dd632aea85cf970b09244107b6d5cea924feb7deafdc37cf9d>, xmr: 1.000000

 - mixin no: 1, block height: 67326, ours: false
  - output's pubkey: <9f3145e43d7e0e3bbeb57d5a2fafef952d315bac341e507645621ed86efd1155>
  - in tx with hash: <21885df01a25c548ddc0bb26dacba7fcc63f8c2810e193d4048fccb9791b1b38>
  - this tx pub key: <6870bc701bea76bd66174216450eaae37166b080d1e7c3db1ffb3d760316f98c>
  - out_i: 175, g_idx: 8381, xmr: 1.000000

 - mixin no: 2, block height: 143814, ours: false
  - output's pubkey: <eeaa6f0a7cc1e975815743f12434b81a921f96c650fe269159dae20ef4077061>
  - in tx with hash: <56aabcd79cd2c063bd40636a2ca69e9933c95e93ec3ae15e93beafbae0293a83>
  - this tx pub key: <ded232fda057e79ecc6353a6a2fd34a3cd0a22053123aa6e35f839f8715b2d99>
  - out_i: 039, g_idx: 132680, xmr: 1.000000

 - mixin no: 3, block height: 153000, ours: false
  - output's pubkey: <ad89c7b09b1f02ad872270510ccb96d24252170cd01bbbb45b307daf89e7ee5c>
  - in tx with hash: <0daef7d911fc62ae50ee0134cb247fcf97061091fcbb1fcf4d96d1c9cdb8a969>
  - this tx pub key: <d841906d5a04d72d4585bd37c23f3501f645f407a392c86d1aaf00ee25842c86>
  - out_i: 036, g_idx: 153950, xmr: 1.000000

 - mixin no: 4, block height: 168055, ours: false
  - output's pubkey: <623eba3d75cc706f34b62c50cc212267e86f50df123741f1deea039da04b6a4e>
  - in tx with hash: <31aa16467530036597f50f07fc30c3c8f8a8df55b19de831fcf3d2c18a951e1f>
  - this tx pub key: <144ce215672c8067b8dc172c2b04ac3e11dfc5fcc027c8ed19b327550dfce532>
  - out_i: 039, g_idx: 180712, xmr: 1.000000

 - mixin no: 5, block height: 277037, ours: false
  - output's pubkey: <f8f1ac2151024887959891d6021d84e6289a02b8743b84f941df15e387fc7c95>
  - in tx with hash: <fc27fd68ca723784c7b9e0459cc9a53779bd1bd317f5a255114d897ecbed7464>
  - this tx pub key: <d535a1fa0eb144794759b6e3adbc6389fce8e04bf172dc08c9dcc2325cb44975>
  - out_i: 202, g_idx: 316577, xmr: 1.000000

 - mixin no: 6, block height: 539756, ours: false
  - output's pubkey: <b81195393cc497f4deff5dbd9f6fc04d4c492e6c523aadb40dbd29cdf38d4662>
  - in tx with hash: <6143d5d3f5ada27c1eddc46176607b67f492b1e9427757174e3e3215eeadaf63>
  - this tx pub key: <b458cc6c05f38339a33036c3ed992997a7511b6148256c6ddf2d6742e9f4c282>
  - out_i: 049, g_idx: 547897, xmr: 1.000000

 - mixin no: 7, block height: 844912, ours: true
  - output's pubkey: <852e2f8c919988294a15a65a4be8adff70e14c5907b0fee7e2ee005c134345e4>
  - in tx with hash: <27003d209ae8854a72735e0cb14f46879dafbac65cf593fb880926a2a674efce>
  - this tx pub key: <70fca5b6be58dbe39ed3fc229bb2a11ccceea89073591a18749c82f816182b37>
  - out_i: 002, g_idx: 654842, xmr: 1.000000
```



## Example input and output 3
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

 - mixin no: 1, block height: 100587, ours: false
  - output's pubkey: <899dbdf74d92ab761d43b9e64881a563d233c352284246dbad856164bce10c56>
  - in tx with hash: <587078cf170458879dfa0f36f1acb5ab8aa3a4757d3159e73364cc81a2604d55>
  - this tx pub key: <fba03d9a2edbcd11526d01653c57d3b097521bcd68877ff3d4f3c541d9cac823>
  - out_i: 087, g_idx: 76852, xmr: 0.070000

 - mixin no: 2, block height: 196153, ours: false
  - output's pubkey: <40cb6dfca1486763fdeb78589b8497e14b0aace19b8042d0fb3b1605c0659ebf>
  - in tx with hash: <d0b3a8cb3144efde755ff1f0a53da08141612a614c76b7045ab449a583dc6abc>
  - this tx pub key: <df553ee9455ce202924f7dc2a00946a810f3764203727a32ca7c71dea769ff94>
  - out_i: 000, g_idx: 103052, xmr: 0.070000

 - mixin no: 3, block height: 419000, ours: false
  - output's pubkey: <1f4e5e32dfb1bf9e51e3858be87b8cdc20ca79b1507b51070cbac3fcf474c84a>
  - in tx with hash: <2e07b521ff7095e4b9c3458b8b406914e971a860182d63687fc9f7030d1598b5>
  - this tx pub key: <8ceb3b90515ff6542f1e5e72d18dee22cc4d3bf4da72f236c453e07cc99cb283>
  - out_i: 005, g_idx: 115440, xmr: 0.070000

 - mixin no: 4, block height: 442769, ours: false
  - output's pubkey: <2c1bf7733129f520e24349f9da1c0dbadfe6faa40020ac4803d516bc2c654ee4>
  - in tx with hash: <88639977921a2dbb7fa9a3d50aedadd49627957f3d726f47342255d26c9d6af1>
  - this tx pub key: <3b386cfeaee3de991540e0694c40e00126ad3eba96b68491de27a44734344725>
  - out_i: 001, g_idx: 117822, xmr: 0.070000

 - mixin no: 5, block height: 536199, ours: false
  - output's pubkey: <099a1522e0c1686f6e7d3739c985949d113a228f297f61ebbcc3430e77072047>
  - in tx with hash: <1783b473caa464b96e2abea1ce6ef0f6047aecc69421b1e3f7677e329be13340>
  - this tx pub key: <d57322ca42afb2c27dea21e668f9269586abbec11097c2bf2507723934b5c579>
  - out_i: 001, g_idx: 129931, xmr: 0.070000

 - mixin no: 6, block height: 597457, ours: false
  - output's pubkey: <6a893bb20ca12a2debb0bc4f6905962cf78a034b8c32cad241a1718fe9cda04d>
  - in tx with hash: <bf22f3291dc7b7f3705a16af4dc725aded208bd63a34fd6df721ad55825ff7c0>
  - this tx pub key: <6e241f6031b09070b1d4cde06d3770bab4c6e4b11a340dacad983ce31fc0af92>
  - out_i: 001, g_idx: 137977, xmr: 0.070000

 - mixin no: 7, block height: 619035, ours: false
  - output's pubkey: <5d898066818e39e9521457e317d930fde698c81c0199112025f68eef01d12419>
  - in tx with hash: <7db677dafa7a83ce6d056e6cf7b2631ac0376e873ef0c34eed4795b66cd3476a>
  - this tx pub key: <9081f7320f046fa51e077d56bb8c3dc305191aefba9a80c09e9f310a84689317>
  - out_i: 001, g_idx: 140897, xmr: 0.070000

 - mixin no: 8, block height: 629805, ours: false
  - output's pubkey: <fc926f529076df29b38396fa8b505026e5b4c59717d675df6913146837c3d49a>
  - in tx with hash: <94421fdf1fcd190d9f01ef27706ee3cf1a6de550522e0a41118f8a62e1dcad67>
  - this tx pub key: <051c018a4564c6ecc6dfa021737117c2c5ba3dfadcbe3b63c02d040233264c96>
  - out_i: 001, g_idx: 142288, xmr: 0.070000

 - mixin no: 9, block height: 719664, ours: false
  - output's pubkey: <3897f183edbc6863ad17c244e5f7847c53456ee6dd96ee0f7e8c69276355a088>
  - in tx with hash: <64f0cc93acbea3f387c6ecc6e863facf2a14a25eec15902e0eda2e466f4dd8b9>
  - this tx pub key: <8e3cce1cfdc1692264bd93a6f9fc2c66deb1642a14c50293fd92c85eed828710>
  - out_i: 001, g_idx: 153185, xmr: 0.070000

 - mixin no: 10, block height: 802102, ours: false
  - output's pubkey: <bb7357d4a834707e3e539b4d58522651103deceaaf2cfadffa4e834e7778fd89>
  - in tx with hash: <0173c66a1fbd2a8b659e3e01ac4c82da1e9b45a0fac59ac566a9b13e51c90699>
  - this tx pub key: <e157acc67cbbcfba9c7733fc0c3667e27f8b869998d428b85d268933bfec8e62>
  - out_i: 001, g_idx: 164180, xmr: 0.070000

 - mixin no: 11, block height: 844908, ours: true
  - output's pubkey: <848a4a76a1d2652fa103ceea9bdd7d4c8a2fcd54cd7050648fa57bb144239642>
  - in tx with hash: <ce26c93d631ff7876134eeaeb574f6e9c791fe3eb816de16f68ee2138c9ccdbb>
  - this tx pub key: <8c730462989bab83bafbfe139ec3603ee47831bd0212185484ac6c22c8a63307>
  - out_i: 001, g_idx: 169601, xmr: 0.070000
```

## Example input and output 4
Transaction hash given along with the corresponding address and view key.

```bash
./showmixins -a 41vEA7Ye8Bpeda6g59v5t46koWrVn2PNgEKgzquJjmiKCFTsh9gajr8J3pad49rqu581TAtFGCH9CYTCkYrCpuWUG9GkgeB -v fed77158ec692fe9eb951f6aeb22c3bda16fe8926c1aac13a5651a9c27f34309  -t b7822d7ae83a07fa49ba54ff935fa6899a49fb5c35269369141470b709c073b8
```

```bash
tx hash          : <b7822d7ae83a07fa49ba54ff935fa6899a49fb5c35269369141470b709c073b8>

private view key : <fed77158ec692fe9eb951f6aeb22c3bda16fe8926c1aac13a5651a9c27f34309>
address          : <41vEA7Ye8Bpeda6g59v5t46koWrVn2PNgEKgzquJjmiKCFTsh9gajr8J3pad49rqu581TAtFGCH9CYTCkYrCpuWUG9GkgeB>


Input's Key image: <68d476d12c23ed8bacf9286469ccce41b3eb51eb3c6be2af5405764093f7e6e2>, xmr: 0.090000

 - mixin no: 1, block height: 108348, ours: false
  - output's pubkey: <7b7d2d572efd85ada54fd7df8af054d04a28a6a0bfeaa7be60ec402faf076888>
  - in tx with hash: <dad81e7c0412ef9d02e308502b02ef2719798a5b2a0f4488a166e90e4e78211b>
  - this tx pub key: <39bcdd1ac483578c72f1875fa01dd3240e05e2fbae13ba9c3dbebdd2d81753c0>
  - out_i: 002, g_idx: 70913, xmr: 0.090000

 - mixin no: 2, block height: 131945, ours: false
  - output's pubkey: <fff8d49e9e912a0ddbcc83a3d23284c4bf7041e9e560341261c2a70bc1db1079>
  - in tx with hash: <449c1642c8ba5c17b1994ad4069bf27499112915e57ff6d73fa1aba68a2af0e4>
  - this tx pub key: <420103c4028645547363f1b19988e29487c9a5c3d0d413e083468423c18bf0da>
  - out_i: 002, g_idx: 82215, xmr: 0.090000

 - mixin no: 3, block height: 485596, ours: false
  - output's pubkey: <5f9e6cb693fdeb3521faf1a81460dde9ba9f89153dcfd5e3474931fc7c0dac48>
  - in tx with hash: <476ccffee2f6087348061782aa6fc583a8a80bd9d491eafd65c8979287efb24b>
  - this tx pub key: <b335c9c2e96b6b8d13f298dbc115d6751c79c56265b6669e6fe9c898dd3e89a3>
  - out_i: 000, g_idx: 144248, xmr: 0.090000

 - mixin no: 4, block height: 844131, ours: true
  - output's pubkey: <5f217820bb13d6f29eb4e889ca85d4abd90411b9b4155bc441b31a159de86cf4>
  - in tx with hash: <d680c60ccc04cea643765428c7c44afb12e7b68cd50fd93182a86b8cb920f3f9>
  - this tx pub key: <b85cfe6c64012476c4a9969e15ed098e9ec5c66caf2c8d3786d16f5d0c4e131d>
  - out_i: 000, g_idx: 200232, xmr: 0.090000

Input's Key image: <7c2aeee6d01cfc71f6fc8b975269fb01b35a8934101672524c35275322a6d949>, xmr: 0.090000

 - mixin no: 1, block height: 115423, ours: false
  - output's pubkey: <f63eb90fefea6abc08ab19a03f6a2b65f98496ed465bcd35532e3cfe0cbba718>
  - in tx with hash: <30726385586ffd850d8c1bf9fbfbfaedee197e30b3e5ddaf1d2c993153c013ae>
  - this tx pub key: <2140baf170b38744a5542e236cf0722b6af5b6d4f9c9399f93ce1d2340ee9b09>
  - out_i: 000, g_idx: 74485, xmr: 0.090000

 - mixin no: 2, block height: 472372, ours: false
  - output's pubkey: <7e2120811ce322caf1981147eb3b7c03cda686d78dbc7367e5259b94f430319c>
  - in tx with hash: <a79749fa9eab22b38ef52d794bf3543982a191b6928640fc6cc0ea04e26e2956>
  - this tx pub key: <f684a30d0f33d59a31963a503345c688d30bb5f327623339288de405458c5de3>
  - out_i: 000, g_idx: 142309, xmr: 0.090000

 - mixin no: 3, block height: 735365, ours: false
  - output's pubkey: <f0fc53e19d2a795dafa2566ab18819a54d9c7c98880bbf639af1eae8938ba01f>
  - in tx with hash: <e827c3d8ee2de1079a4f0aefb12f6b44d7100b2326ebf522640c126370578fdb>
  - this tx pub key: <3a165581f3e577582f4860c924bb7a8dabf89a3e8e1bef173d9a586e41e2c38e>
  - out_i: 002, g_idx: 183927, xmr: 0.090000

 - mixin no: 4, block height: 844131, ours: true
  - output's pubkey: <5cb0a097d4f13062756ecd11c324c6c11eca2d8ebf90241bdc891a3ae08d1365>
  - in tx with hash: <c772702301bea767352f6309f0f46bf8b84a105580a6cdfdf30d16b073f3b2fe>
  - this tx pub key: <ef0a0ec9c3a3fd91d9ede9a2b6d7865464d952c0ec0fbfe6393cdeeda15753e7>
  - out_i: 000, g_idx: 200233, xmr: 0.090000
```

## Observation

Have you noticed something interesting in the above examples? 


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
