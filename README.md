# Show detailed mixins information for a given transaction

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

    // .....
    // argument parsing removed to save some space
    // .....

    // create instance of our MicroCore
    xmreg::MicroCore mcore;

    // initialize the core using the blockchain path
    if (!mcore.init(blockchain_path.string()))
    {
        cerr << "Error accessing blockchain." << endl;
        return 1;
    }

    // get the high level cryptonote::Blockchain object to interact
    // with the blockchain lmdb database
    cryptonote::Blockchain& core_storage = mcore.get_core();


    // get the current blockchain height. Just to check
    // if it reads ok.
    uint64_t height = core_storage.get_current_blockchain_height() - 1;

    print("\n\n"
          "Top block height      : {:d}\n", height);

    // get time of the current block
    uint64_t current_blk_timestamp = mcore.get_blk_timestamp(height);

    print("Top block block time  : {:s}\n", xmreg::timestamp_to_str(current_blk_timestamp));


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

    // get block height in which the given transaction is located
    uint64_t tx_blk_height = core_storage.get_db().get_tx_block_height(tx_hash);

    print("\ntx hash          : {}, block height {}\n\n", tx_hash, tx_blk_height);

    if (VIEWKEY_AND_ADDRESS_GIVEN)
    {
        // lets check our keys
        print("private view key : {}\n", private_view_key);
        print("address          : {}\n\n\n", xmreg::print_address(address, testnet));
    }

    // total number of inputs in the transaction tx
    size_t input_no = tx.vin.size();

    for (size_t in_i = 0; in_i < input_no; ++in_i)
    {
        cryptonote::txin_v tx_in = tx.vin[in_i];

        if (tx_in.type() == typeid(cryptonote::txin_gen))
        {
            print(" - coinbase tx: no inputs here.\n");
            continue;
        }

        // get tx input key
        const cryptonote::txin_to_key& tx_in_to_key
                = boost::get<cryptonote::txin_to_key>(tx_in);


        print("Input's key image: {}, xmr: {:0.6f}\n",
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


            // get block of given height, as we want to get its timestamp
            cryptonote::block blk;

            if (!mcore.get_block_by_height(output_data.height, blk))
            {
                print("- cant get block of height: {}\n", output_data.height);
                continue;
            }

            // get mixin block timestamp
            uint64_t blk_timestamp = blk.timestamp;

            // calculate time difference bewteen mixing block and current blockchain height
            array<size_t, 5> time_diff;
            time_diff = xmreg::timestamp_difference(current_blk_timestamp, blk_timestamp);


            print("\n - mixin no: {}, block height: {}, timestamp: {}, "
                          "time_diff: {} y, {} d, {} h, {} m, {} s",
                  count + 1, output_data.height,
                  xmreg::timestamp_to_str(blk_timestamp),
                  time_diff[0], time_diff[1], time_diff[2], time_diff[3], time_diff[4]);

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
        } // for (const uint64_t& i: absolute_offsets)

        print("\nRing signature for the above input, i.e.,: key image {}, xmr: {:0.6f}: \n",
              tx_in_to_key.k_image, xmreg::get_xmr(tx_in_to_key.amount));

        for (const crypto::signature &sig: tx.signatures[in_i])
        {
            cout << " - " << xmreg::print_sig(sig) << endl;
        }

        cout << endl;

    } // for (size_t in_i = 0; in_i < input_no; ++in_i)

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
Top block height      : 1027565
Top block block time  : 2016-04-17 18:50:37

Payment id: not present

tx hash          : <49503c381ed74da2079697f0e8b7228608da3cade22575774ab8cf5ca425c3fe>, block height 844932

Input's key image: <54802347b456a6dd632aea85cf970b09244107b6d5cea924feb7deafdc37cf9d>, xmr: 1.00000000

 - mixin no: 1, block height: 67326, timestamp: 2014-06-02 16:58:23, time_diff: 1 y, 320 d, 1 h, 52 m, 14 s
  - output's pubkey: <9f3145e43d7e0e3bbeb57d5a2fafef952d315bac341e507645621ed86efd1155>
  - in tx with hash: <21885df01a25c548ddc0bb26dacba7fcc63f8c2810e193d4048fccb9791b1b38>
  - this tx pub key: <6870bc701bea76bd66174216450eaae37166b080d1e7c3db1ffb3d760316f98c>
  - out_i: 175, g_idx: 8381, xmr: 1.00000000

 - mixin no: 2, block height: 143814, timestamp: 2014-07-25 13:31:17, time_diff: 1 y, 267 d, 5 h, 19 m, 20 s
  - output's pubkey: <eeaa6f0a7cc1e975815743f12434b81a921f96c650fe269159dae20ef4077061>
  - in tx with hash: <56aabcd79cd2c063bd40636a2ca69e9933c95e93ec3ae15e93beafbae0293a83>
  - this tx pub key: <ded232fda057e79ecc6353a6a2fd34a3cd0a22053123aa6e35f839f8715b2d99>
  - out_i: 039, g_idx: 132680, xmr: 1.00000000

 - mixin no: 3, block height: 153000, timestamp: 2014-07-31 21:51:33, time_diff: 1 y, 260 d, 20 h, 59 m, 4 s
  - output's pubkey: <ad89c7b09b1f02ad872270510ccb96d24252170cd01bbbb45b307daf89e7ee5c>
  - in tx with hash: <0daef7d911fc62ae50ee0134cb247fcf97061091fcbb1fcf4d96d1c9cdb8a969>
  - this tx pub key: <d841906d5a04d72d4585bd37c23f3501f645f407a392c86d1aaf00ee25842c86>
  - out_i: 036, g_idx: 153950, xmr: 1.00000000

 - mixin no: 4, block height: 168055, timestamp: 2014-08-11 11:27:23, time_diff: 1 y, 250 d, 7 h, 23 m, 14 s
  - output's pubkey: <623eba3d75cc706f34b62c50cc212267e86f50df123741f1deea039da04b6a4e>
  - in tx with hash: <31aa16467530036597f50f07fc30c3c8f8a8df55b19de831fcf3d2c18a951e1f>
  - this tx pub key: <144ce215672c8067b8dc172c2b04ac3e11dfc5fcc027c8ed19b327550dfce532>
  - out_i: 039, g_idx: 180712, xmr: 1.00000000

 - mixin no: 5, block height: 277037, timestamp: 2014-10-26 13:04:02, time_diff: 1 y, 174 d, 5 h, 46 m, 35 s
  - output's pubkey: <f8f1ac2151024887959891d6021d84e6289a02b8743b84f941df15e387fc7c95>
  - in tx with hash: <fc27fd68ca723784c7b9e0459cc9a53779bd1bd317f5a255114d897ecbed7464>
  - this tx pub key: <d535a1fa0eb144794759b6e3adbc6389fce8e04bf172dc08c9dcc2325cb44975>
  - out_i: 202, g_idx: 316577, xmr: 1.00000000

 - mixin no: 6, block height: 539756, timestamp: 2015-04-28 08:58:05, time_diff: 0 y, 355 d, 9 h, 52 m, 32 s
  - output's pubkey: <b81195393cc497f4deff5dbd9f6fc04d4c492e6c523aadb40dbd29cdf38d4662>
  - in tx with hash: <6143d5d3f5ada27c1eddc46176607b67f492b1e9427757174e3e3215eeadaf63>
  - this tx pub key: <b458cc6c05f38339a33036c3ed992997a7511b6148256c6ddf2d6742e9f4c282>
  - out_i: 049, g_idx: 547897, xmr: 1.00000000

 - mixin no: 7, block height: 844912, timestamp: 2015-11-28 11:22:49, time_diff: 0 y, 141 d, 7 h, 27 m, 48 s
  - output's pubkey: <852e2f8c919988294a15a65a4be8adff70e14c5907b0fee7e2ee005c134345e4>
  - in tx with hash: <27003d209ae8854a72735e0cb14f46879dafbac65cf593fb880926a2a674efce>
  - this tx pub key: <70fca5b6be58dbe39ed3fc229bb2a11ccceea89073591a18749c82f816182b37>
  - out_i: 002, g_idx: 654842, xmr: 1.00000000

Ring signature for the above input, i.e.,: key image <54802347b456a6dd632aea85cf970b09244107b6d5cea924feb7deafdc37cf9d>, xmr: 1.00000000:

 - c: <65b85a3e6e90979db84abcff8972f14143f75af3502dd078292ed42f0b6b7107> r: <2d12c4790b885e530081ece690a3a0722a23d1a428aa2a0a20d4c1feb549ee05>
 - c: <5804d88fdd7b992f06d62ccce5bf71ae4068d4ea7f7520e82863afce6c09a505> r: <c8dad370582e29bfe4976c30e2305cc0a1734d98ceb6f0f41de45ce9b5642809>
 - c: <7745f72b61daa2be00b9961cb5af10b4cc665e4bcb94e59b4be9cc83195ae70b> r: <b0ce4471e409682ca03353fa66af69ee0689ee3665c87a3b46391329f405d805>
 - c: <6877ccfe2c8abda3669e59c415ef6da98c9f0231d8aec0b9d75a18d16511050c> r: <be399b3874caa25face98fc1564b6151b61eac1767318fc625f7713aa59f5801>
 - c: <4e50fbfa5118a66d0ee41564c86cd7b737719b9958f6428ab911069f6dbf2001> r: <abce50fcea4411362dc800ac5b45d4caef175e3b8a58c27c795fbe29cd8baa0f>
 - c: <228b4af5f4db071d8c0033dd774e9b06b2be0929d60d8c738be614c111536d09> r: <c0d55c26077da9aaa559f3f20ac6cc23604aea4366dac47cb8ef860e28efed0e>
 - c: <f2df5b723c21e4770ed7bc765799e4a06f95cd5c24ce5fa25e4f12e64fde970e> r: <2652bbf2e207084d89fd72507d19271b86bf2e7e1a26c04aa53a4cc594fd200f>


Mixin timescales for this transaction:

Genesis <_______*________***____________*_____________________________*__________________________________*________________________> 2016-04-17
```

## Example input and output 2 (mixin 6)
Transaction hash given along with the corresponding address and view key.

```bash
./showmixins -a 42SLnbz1Ym9YtU3zHwKbQL8hXnGhtQ23BdnTGBtWwj6AXHSdEqRikDbM3wQxDWMhyCKZbQ9TfFh9N1SvHMXT81kK7senkME -v bdee822e89095833315925543a6d5b2a2a4418815cdfb3d0e91722d9c0b79501 -t 49503c381ed74da2079697f0e8b7228608da3cade22575774ab8cf5ca425c3fe
```

Result:
```bash

Top block height      : 1027565
Top block block time  : 2016-04-17 18:50:37

Payment id: not present

tx hash          : <49503c381ed74da2079697f0e8b7228608da3cade22575774ab8cf5ca425c3fe>, block height 844932

private view key : <bdee822e89095833315925543a6d5b2a2a4418815cdfb3d0e91722d9c0b79501>
address          : <42SLnbz1Ym9YtU3zHwKbQL8hXnGhtQ23BdnTGBtWwj6AXHSdEqRikDbM3wQxDWMhyCKZbQ9TfFh9N1SvHMXT81kK7senkME>


Input's key image: <54802347b456a6dd632aea85cf970b09244107b6d5cea924feb7deafdc37cf9d>, xmr: 1.00000000

 - mixin no: 1, block height: 67326, timestamp: 2014-06-02 16:58:23, time_diff: 1 y, 320 d, 1 h, 52 m, 14 s, ours: false
  - output's pubkey: <9f3145e43d7e0e3bbeb57d5a2fafef952d315bac341e507645621ed86efd1155>
  - in tx with hash: <21885df01a25c548ddc0bb26dacba7fcc63f8c2810e193d4048fccb9791b1b38>
  - this tx pub key: <6870bc701bea76bd66174216450eaae37166b080d1e7c3db1ffb3d760316f98c>
  - out_i: 175, g_idx: 8381, xmr: 1.00000000

 - mixin no: 2, block height: 143814, timestamp: 2014-07-25 13:31:17, time_diff: 1 y, 267 d, 5 h, 19 m, 20 s, ours: false
  - output's pubkey: <eeaa6f0a7cc1e975815743f12434b81a921f96c650fe269159dae20ef4077061>
  - in tx with hash: <56aabcd79cd2c063bd40636a2ca69e9933c95e93ec3ae15e93beafbae0293a83>
  - this tx pub key: <ded232fda057e79ecc6353a6a2fd34a3cd0a22053123aa6e35f839f8715b2d99>
  - out_i: 039, g_idx: 132680, xmr: 1.00000000

 - mixin no: 3, block height: 153000, timestamp: 2014-07-31 21:51:33, time_diff: 1 y, 260 d, 20 h, 59 m, 4 s, ours: false
  - output's pubkey: <ad89c7b09b1f02ad872270510ccb96d24252170cd01bbbb45b307daf89e7ee5c>
  - in tx with hash: <0daef7d911fc62ae50ee0134cb247fcf97061091fcbb1fcf4d96d1c9cdb8a969>
  - this tx pub key: <d841906d5a04d72d4585bd37c23f3501f645f407a392c86d1aaf00ee25842c86>
  - out_i: 036, g_idx: 153950, xmr: 1.00000000

 - mixin no: 4, block height: 168055, timestamp: 2014-08-11 11:27:23, time_diff: 1 y, 250 d, 7 h, 23 m, 14 s, ours: false
  - output's pubkey: <623eba3d75cc706f34b62c50cc212267e86f50df123741f1deea039da04b6a4e>
  - in tx with hash: <31aa16467530036597f50f07fc30c3c8f8a8df55b19de831fcf3d2c18a951e1f>
  - this tx pub key: <144ce215672c8067b8dc172c2b04ac3e11dfc5fcc027c8ed19b327550dfce532>
  - out_i: 039, g_idx: 180712, xmr: 1.00000000

 - mixin no: 5, block height: 277037, timestamp: 2014-10-26 13:04:02, time_diff: 1 y, 174 d, 5 h, 46 m, 35 s, ours: false
  - output's pubkey: <f8f1ac2151024887959891d6021d84e6289a02b8743b84f941df15e387fc7c95>
  - in tx with hash: <fc27fd68ca723784c7b9e0459cc9a53779bd1bd317f5a255114d897ecbed7464>
  - this tx pub key: <d535a1fa0eb144794759b6e3adbc6389fce8e04bf172dc08c9dcc2325cb44975>
  - out_i: 202, g_idx: 316577, xmr: 1.00000000

 - mixin no: 6, block height: 539756, timestamp: 2015-04-28 08:58:05, time_diff: 0 y, 355 d, 9 h, 52 m, 32 s, ours: false
  - output's pubkey: <b81195393cc497f4deff5dbd9f6fc04d4c492e6c523aadb40dbd29cdf38d4662>
  - in tx with hash: <6143d5d3f5ada27c1eddc46176607b67f492b1e9427757174e3e3215eeadaf63>
  - this tx pub key: <b458cc6c05f38339a33036c3ed992997a7511b6148256c6ddf2d6742e9f4c282>
  - out_i: 049, g_idx: 547897, xmr: 1.00000000

 - mixin no: 7, block height: 844912, timestamp: 2015-11-28 11:22:49, time_diff: 0 y, 141 d, 7 h, 27 m, 48 s, ours: true
  - output's pubkey: <852e2f8c919988294a15a65a4be8adff70e14c5907b0fee7e2ee005c134345e4>
  - in tx with hash: <27003d209ae8854a72735e0cb14f46879dafbac65cf593fb880926a2a674efce>
  - this tx pub key: <70fca5b6be58dbe39ed3fc229bb2a11ccceea89073591a18749c82f816182b37>
  - out_i: 002, g_idx: 654842, xmr: 1.00000000

Ring signature for the above input, i.e.,: key image <54802347b456a6dd632aea85cf970b09244107b6d5cea924feb7deafdc37cf9d>, xmr: 1.00000000:

 - c: <65b85a3e6e90979db84abcff8972f14143f75af3502dd078292ed42f0b6b7107> r: <2d12c4790b885e530081ece690a3a0722a23d1a428aa2a0a20d4c1feb549ee05>
 - c: <5804d88fdd7b992f06d62ccce5bf71ae4068d4ea7f7520e82863afce6c09a505> r: <c8dad370582e29bfe4976c30e2305cc0a1734d98ceb6f0f41de45ce9b5642809>
 - c: <7745f72b61daa2be00b9961cb5af10b4cc665e4bcb94e59b4be9cc83195ae70b> r: <b0ce4471e409682ca03353fa66af69ee0689ee3665c87a3b46391329f405d805>
 - c: <6877ccfe2c8abda3669e59c415ef6da98c9f0231d8aec0b9d75a18d16511050c> r: <be399b3874caa25face98fc1564b6151b61eac1767318fc625f7713aa59f5801>
 - c: <4e50fbfa5118a66d0ee41564c86cd7b737719b9958f6428ab911069f6dbf2001> r: <abce50fcea4411362dc800ac5b45d4caef175e3b8a58c27c795fbe29cd8baa0f>
 - c: <228b4af5f4db071d8c0033dd774e9b06b2be0929d60d8c738be614c111536d09> r: <c0d55c26077da9aaa559f3f20ac6cc23604aea4366dac47cb8ef860e28efed0e>
 - c: <f2df5b723c21e4770ed7bc765799e4a06f95cd5c24ce5fa25e4f12e64fde970e> r: <2652bbf2e207084d89fd72507d19271b86bf2e7e1a26c04aa53a4cc594fd200f>


Mixin timescales for this transaction:

Genesis <_______*________***____________*_____________________________*__________________________________*________________________> 2016-04-17
```

## Example input and output 3 (mixin 10)
Transaction hash given along with the corresponding address and view key.
```bash
./showmixins 1c01255462843f79f398a2e0678ee289e42fa2aea4a1447411e3820bd5fa62b9
```

```bash
Top block height      : 1027568
Top block block time  : 2016-04-17 18:57:46

Payment id: <66d404452b03e03e4307dae8e3eb4776fbbaec68e1dcb061a0748e39667271e7>

tx hash          : <1c01255462843f79f398a2e0678ee289e42fa2aea4a1447411e3820bd5fa62b9>, block height 1026846

Input's key image: <5d55aa0ed8ea271992163bcc88bdd3935e8a3d5631bf3d737754a28762ef816e>, xmr: 0.20000000

 - mixin no: 1, block height: 101988, timestamp: 2014-06-26 13:56:26, time_diff: 1 y, 296 d, 5 h, 1 m, 20 s
  - output's pubkey: <aedfe1f74a4a709cfbe7ce1f1f0cb78dfeec65304b46dfaf1fe3000c9dc15f70>
  - in tx with hash: <0c019e5c4005cb3ea2a4effa5eb17b0f1448640bdbc7d5c97b5a05e4a2e63c20>
  - this tx pub key: <a07fb034048e2fddb95aaf2cdae6e2db8ae03ca4c944a65a4a1f8945d13b35f5>
  - out_i: 014, g_idx: 90788, xmr: 0.20000000

 - mixin no: 2, block height: 129341, timestamp: 2014-07-15 14:00:29, time_diff: 1 y, 277 d, 4 h, 57 m, 17 s
  - output's pubkey: <63c01fd955b121fa1d39719689c162e98d7ff8dd59585e62a8fecc0c84f8e817>
  - in tx with hash: <ea636aced6766095bc1a59f20464f6a42189e629d63fbcff0f3abf56a0f4cbbd>
  - this tx pub key: <c1f351d707f253fe4274dfaf4182761583ca6c9d49fb7a88c75837f70beee9a1>
  - out_i: 009, g_idx: 198989, xmr: 0.20000000

 - mixin no: 3, block height: 135680, timestamp: 2014-07-19 22:07:51, time_diff: 1 y, 272 d, 20 h, 49 m, 55 s
  - output's pubkey: <d5d8368fe568beec11a8fa7fc56262f4207a4e8b81c678a2997c1b4a0a352121>
  - in tx with hash: <0aa844874dd029d3448865e58e1d29b267152dfec09ebe4dae76639339ef7aed>
  - this tx pub key: <0f4d742ccf37ddfe23d3bf0158d6c4f0e2c0583db305f29d023996a8d8a1d04e>
  - out_i: 001, g_idx: 221962, xmr: 0.20000000

 - mixin no: 4, block height: 135964, timestamp: 2014-07-20 02:34:50, time_diff: 1 y, 272 d, 16 h, 22 m, 56 s
  - output's pubkey: <29e12f8fc819841d101ca82643456e3d75e09c4b1fb9ade60d16a273bd2a1b33>
  - in tx with hash: <8642a866572cb38c2f26707a548f55617004762d9e2601bce02465305b3ffd0e>
  - this tx pub key: <824903b1f82feaa64e655261480c37a666e1ffda899a6140017223b1c59e1b5c>
  - out_i: 012, g_idx: 223142, xmr: 0.20000000

 - mixin no: 5, block height: 137469, timestamp: 2014-07-21 03:42:31, time_diff: 1 y, 271 d, 15 h, 15 m, 15 s
  - output's pubkey: <db6419521c97d15fadb3e91ee5032e965dfe4a6621d10c509da23d0a94b5377d>
  - in tx with hash: <c83e76da5f8404fcfa0ad2f7af31e150806320aca25577d6a6d3e71d8d015958>
  - this tx pub key: <fb461d44547051027cd234a6f9744bca1e5a2c0d7de350cf084f61e2a77e2e30>
  - out_i: 032, g_idx: 229259, xmr: 0.20000000

 - mixin no: 6, block height: 139122, timestamp: 2014-07-22 07:43:17, time_diff: 1 y, 270 d, 11 h, 14 m, 29 s
  - output's pubkey: <b5de0b2f841bd74c92f8a43d5d8d31338f2ef797570a942d652ddd1aabe4d4ae>
  - in tx with hash: <5801df918d06937dd07329c3032bfd51d7986f1f6d6f63e6dc1f3e4acf43250f>
  - this tx pub key: <9e3d3b58f3a0b520541c801388456e518aed70a5cb79375cab5ae55b5b405a06>
  - out_i: 024, g_idx: 235993, xmr: 0.20000000

 - mixin no: 7, block height: 147512, timestamp: 2014-07-28 03:20:01, time_diff: 1 y, 264 d, 15 h, 37 m, 45 s
  - output's pubkey: <290bcb5f8b7e3805307826f4adab95ebb0c090aa3b7bc04427dca639991b6c03>
  - in tx with hash: <564977d88c9472ded7d2442e86a0d749ef9fefc02afee3eefdfb0344b580ec80>
  - this tx pub key: <2bc87a6defaf6c0b9392c8645afc3eeaf68cdd4453f4a04bcd46525f80570d08>
  - out_i: 002, g_idx: 268944, xmr: 0.20000000

 - mixin no: 8, block height: 184529, timestamp: 2014-08-22 20:54:37, time_diff: 1 y, 238 d, 22 h, 3 m, 9 s
  - output's pubkey: <8a935971e59f140e5e279c244cbada9861072f9982888a4d4baa09ec1f50b6a8>
  - in tx with hash: <3e9bb3ddf41eca1cd23cb7fcf3556467a588e7264d940e35d1c79ba5e56184fa>
  - this tx pub key: <a346e3623bbc63992ee94d5d3bd8a4b312588f6847342fb25ecf04761d6c9002>
  - out_i: 030, g_idx: 396948, xmr: 0.20000000

 - mixin no: 9, block height: 202543, timestamp: 2014-09-04 11:22:44, time_diff: 1 y, 226 d, 7 h, 35 m, 2 s
  - output's pubkey: <54b610fc1d2a1b8087ed97c27394ffe170387d83c281abd09827620d032ee2e4>
  - in tx with hash: <7d6a26b25614d02a10259e4390d0aeaf61bd82c6912c50bed2e8d6f0a3acac0b>
  - this tx pub key: <a57506401d2d1ceaa75802689791e7b006559a8d7ad555bf0ec5c39fe74a49fe>
  - out_i: 019, g_idx: 464387, xmr: 0.20000000

 - mixin no: 10, block height: 261400, timestamp: 2014-10-15 13:54:14, time_diff: 1 y, 185 d, 5 h, 3 m, 32 s
  - output's pubkey: <a37496e3f1fffcba8f9eed08bdb0c35db596bf33a93ee719ed362a57e0a6e151>
  - in tx with hash: <a0dc04ea9bfc30b5de7203e09ba8778e5d7205be53f158f1d3a43df26f955c14>
  - this tx pub key: <5e2fde8943c0d1864c1b886ae3e36aee64a5d94eec1776f92cf0a3197f23a1d0>
  - out_i: 054, g_idx: 562931, xmr: 0.20000000

 - mixin no: 11, block height: 266414, timestamp: 2014-10-19 00:14:39, time_diff: 1 y, 181 d, 18 h, 43 m, 7 s
  - output's pubkey: <5fc6ecf73c237f3754dd07f32cdffbae79e65d5ed73bb811605ccf4735396a92>
  - in tx with hash: <fb54cedfa3e1dfbccd38e8250cfeb399b68eb8c274ebe64c043db95b9e194012>
  - this tx pub key: <6fae5c08c5d3cb9dbc67fb5795cbcd77ec1001a8abe7dbb58d549308d86b5eb3>
  - out_i: 056, g_idx: 568933, xmr: 0.20000000

 - mixin no: 12, block height: 297938, timestamp: 2014-11-10 05:29:29, time_diff: 1 y, 159 d, 13 h, 28 m, 17 s
  - output's pubkey: <5426f3743732a249b27001a6440eaa337e1b91e102953d2d1e0a70c7a97360e9>
  - in tx with hash: <27c4f241697bcb1bbb21233fbcd4ff04a3d249e894ef1f4ace09fded0c6fa98d>
  - this tx pub key: <57b8907d1047cee8280c15a11f2b23eb4a7cfbfdf665ba4b5ad20b0600b90c48>
  - out_i: 000, g_idx: 607533, xmr: 0.20000000

 - mixin no: 13, block height: 297959, timestamp: 2014-11-10 05:53:47, time_diff: 1 y, 159 d, 13 h, 3 m, 59 s
  - output's pubkey: <77615f98c696570e33f03956bb9f0e11f60c0d327116db26b3c45303a387d8fe>
  - in tx with hash: <294e9d4a179c80436abe0b1b0ea701a8a01f514d5e93bb4c82181f2b949ec888>
  - this tx pub key: <04d50cdd0e606ce992305c8d2f0595db275dfcb855cfc2fe5709f001f7a86cc7>
  - out_i: 020, g_idx: 607571, xmr: 0.20000000

 - mixin no: 14, block height: 311369, timestamp: 2014-11-19 15:13:04, time_diff: 1 y, 150 d, 3 h, 44 m, 42 s
  - output's pubkey: <2b689150b98478e93b18eaf15fdaa29009a7806747045a38bb0ea7fd913e51e4>
  - in tx with hash: <775d27cf11a446899697d7a93300d07b5e7ad1b74fa892380fd12c5ed1195657>
  - this tx pub key: <f100a6f949e23975e583759d0a920b41bd003270131524af2c4cc28869a9a115>
  - out_i: 054, g_idx: 629180, xmr: 0.20000000

 - mixin no: 15, block height: 315015, timestamp: 2014-11-22 03:00:48, time_diff: 1 y, 147 d, 15 h, 56 m, 58 s
  - output's pubkey: <34e5f09440689c3fd7e6eb9205ca20e53057f2c0592555618a1528d7322e354c>
  - in tx with hash: <b47b55cf1310b5e8057df90c6aeebe9c2c2cd84101630b91b5ed0ba977aa2e26>
  - this tx pub key: <a394bb7b3f84a8a475367f6b23a01b5738db6d7b77f43dbc72acfd1dc92f0f99>
  - out_i: 010, g_idx: 632024, xmr: 0.20000000

 - mixin no: 16, block height: 346336, timestamp: 2014-12-14 01:42:19, time_diff: 1 y, 125 d, 17 h, 15 m, 27 s
  - output's pubkey: <c778a9ade24cf648a11b3181a2fa8447826115385702c0b6d8b80f35239e6c5e>
  - in tx with hash: <dc635914ad08f1ef4c6c4baeeb0e7afe875a2c98bdc09095767ca8915a3abfc2>
  - this tx pub key: <25db49e4d1d1ac378966a8936b5882825e6e892090993428c3c8222d3c3d1fda>
  - out_i: 008, g_idx: 678554, xmr: 0.20000000

 - mixin no: 17, block height: 352106, timestamp: 2014-12-18 03:25:36, time_diff: 1 y, 121 d, 15 h, 32 m, 10 s
  - output's pubkey: <560ed880d99c973743926eff7eb9e3c6ac4b64fe6b163081cc1cee1c7ddb26b0>
  - in tx with hash: <959b573ba4ae15270eb3e97b3d5312aacec82aec51520b8eb4f37f102b1a6655>
  - this tx pub key: <ad4a6ffa137d6334973adbc26995ddc2c2cb15aca60790d1c0f6dda0c5b000b7>
  - out_i: 014, g_idx: 685140, xmr: 0.20000000

 - mixin no: 18, block height: 371148, timestamp: 2014-12-31 13:06:23, time_diff: 1 y, 108 d, 5 h, 51 m, 23 s
  - output's pubkey: <b5148138001848cca219761098dcf82989582f562c0d36006139193578440d1c>
  - in tx with hash: <9ff722644867c870bae0da0db4a4acbf6ae3187adcc0bd5a8836e73966637a3f>
  - this tx pub key: <af7e6a34cba172f2b5c6edb0c46559fd7582ccceb145abeea8cf4195c9b7c17e>
  - out_i: 008, g_idx: 709323, xmr: 0.20000000

 - mixin no: 19, block height: 373952, timestamp: 2015-01-02 12:35:02, time_diff: 1 y, 106 d, 6 h, 22 m, 44 s
  - output's pubkey: <c54144b1479fbd3e7f4e24ea394cf9fd59cf4e6587da8a5a391f1c5d94c6bc4c>
  - in tx with hash: <1854eda26ee569a537083570287f307d5741dac4daaded200682465a3649ef4d>
  - this tx pub key: <8c13d252a9e3731727df035381d40e8c302ef5860e7aca42753c3aa37112f5d4>
  - out_i: 022, g_idx: 710880, xmr: 0.20000000

 - mixin no: 20, block height: 376207, timestamp: 2015-01-04 01:16:03, time_diff: 1 y, 104 d, 17 h, 41 m, 43 s
  - output's pubkey: <4a3317bf48391e7012e44cc88ac7f91f097b95bfe2f527bf3defac60c957261d>
  - in tx with hash: <1542731093706e150601b0ce6f98aa873836a8eadf215b328d076d6cc8a63f20>
  - this tx pub key: <d12df02d5c1868df5163f27dd2e7ff12ad79fd8a9bed45d6c72e1071ae89f41c>
  - out_i: 020, g_idx: 713412, xmr: 0.20000000

 - mixin no: 21, block height: 379158, timestamp: 2015-01-06 02:50:35, time_diff: 1 y, 102 d, 16 h, 7 m, 11 s
  - output's pubkey: <296fb74170b222be553395a5e0d74444dbde8e33cb0a68bd4a405a964e485220>
  - in tx with hash: <1e3abc9dcc911064526de76430971de49555243191ac070f5be4d322fd5ea3a0>
  - this tx pub key: <0b0493c0ee191fb2d4a42969716494f1ff8b2baa1307ceabb5f76a2f614c34c3>
  - out_i: 006, g_idx: 718222, xmr: 0.20000000

 - mixin no: 22, block height: 400200, timestamp: 2015-01-20 21:19:24, time_diff: 1 y, 87 d, 21 h, 38 m, 22 s
  - output's pubkey: <90f29190b10621f7af51e223ce58cb635a32c16528dab6eb4074e42153fb333f>
  - in tx with hash: <66546cb5a7f2be477868eb8214088008d4841f43f1d196d3907f17a9252a0859>
  - this tx pub key: <47fb21ee0e5bf97b6f363a2fa1bd658d0b8cfe870b12f898072fbd72cfcb9a0b>
  - out_i: 027, g_idx: 746154, xmr: 0.20000000

 - mixin no: 23, block height: 429004, timestamp: 2015-02-10 00:45:22, time_diff: 1 y, 67 d, 18 h, 12 m, 24 s
  - output's pubkey: <fc3cb6ba7ea669138673b4dc8e74fa088523f2938018bd4737dcb5b42903bb2a>
  - in tx with hash: <ccf85dc8908146be2429eb6594af75c987128021f8dcdf4e72a097c9bc308b05>
  - this tx pub key: <e96a28097d5bf9b3ae6c24a700a1927dd49af805d70d78e049612df61257964f>
  - out_i: 013, g_idx: 776161, xmr: 0.20000000

 - mixin no: 24, block height: 446193, timestamp: 2015-02-22 00:44:03, time_diff: 1 y, 55 d, 18 h, 13 m, 43 s
  - output's pubkey: <931efdaa36f24c4327041f0cb94cccdf3ffc0dfcd639e265e57d5d16ff00cc5e>
  - in tx with hash: <13fa1e5dca4f23ebec36ba606acc5c1b7ea9b7af5bb1dc2333ad658b6de887fc>
  - this tx pub key: <a6074182da0df0aa24bb149d49cdd3e3ce0d77819b23f27fc2650e7a4211abd9>
  - out_i: 011, g_idx: 792416, xmr: 0.20000000

 - mixin no: 25, block height: 447562, timestamp: 2015-02-22 23:03:22, time_diff: 1 y, 54 d, 19 h, 54 m, 24 s
  - output's pubkey: <706be2ca0d155f7f32ba8762b853a9578199946cd17b814f23fa9954030d4a17>
  - in tx with hash: <39f07022022961382e821a6ee0cce7d2fbae88c08d6b038c99683b18dfea74bc>
  - this tx pub key: <580fc5ca0b2aeb79096478f416d894120267c8228958d40398287abb8acf7101>
  - out_i: 005, g_idx: 793799, xmr: 0.20000000

 - mixin no: 26, block height: 468379, timestamp: 2015-03-09 13:35:45, time_diff: 1 y, 40 d, 5 h, 22 m, 1 s
  - output's pubkey: <06e7d47d6f24a9c64143e62d08543f1dc4861ff3eabd7d5428c906d05ed12ab9>
  - in tx with hash: <34ee7f03db0739baf8f3bd2ff2120d14c1e28b309faf8b4c19cb7172771de8e8>
  - this tx pub key: <0f86302c09cafd8fc12a05e4fde70dbeca9a5bec7fc7cc1b834d3f21421f19e8>
  - out_i: 005, g_idx: 815891, xmr: 0.20000000

 - mixin no: 27, block height: 493800, timestamp: 2015-03-27 07:05:14, time_diff: 1 y, 22 d, 11 h, 52 m, 32 s
  - output's pubkey: <c7041635ea1e6bb2aa7df1d35a20d8b8f62c4e5dd83d6c77cb2769d20ad2a48a>
  - in tx with hash: <0e11bf1b20a1e3cb34c1a146825204c20b5f6dd7b32491649b96a7def32879d0>
  - this tx pub key: <30a06dbe327fa826142e4b564bdfa11d54668e0ccfc7fc6c43d0f9206474b3ed>
  - out_i: 003, g_idx: 843788, xmr: 0.20000000

 - mixin no: 28, block height: 509651, timestamp: 2015-04-07 07:58:08, time_diff: 1 y, 11 d, 10 h, 59 m, 38 s
  - output's pubkey: <6849397a44fc2ffecb0b181b2422be16c59b75ac95948213b8984758f2c9f77e>
  - in tx with hash: <e9fe0a43df1a560dae44eecada579551e40692254d7e28ea457cb5f53dd96cae>
  - this tx pub key: <8274c848eeff03d0aa236400dec9e306d8ff94b73325f77fe6ff2c67fc0250e6>
  - out_i: 005, g_idx: 855276, xmr: 0.20000000

 - mixin no: 29, block height: 567101, timestamp: 2015-05-17 11:55:23, time_diff: 0 y, 336 d, 7 h, 2 m, 23 s
  - output's pubkey: <aa07b9bd81ba3d7b37f2766a4599568c1a5408e1e9b31f9495a11212460f61a1>
  - in tx with hash: <73c0a8f692b209acad1a18b81afc81f4cedfbe38fd022aa6af7553de9cc68819>
  - this tx pub key: <2ab004526bff02dbf98cf511a715d9ac162e330223fae4f98acc274b917d940b>
  - out_i: 002, g_idx: 893421, xmr: 0.20000000

 - mixin no: 30, block height: 593704, timestamp: 2015-06-05 01:48:28, time_diff: 0 y, 317 d, 17 h, 9 m, 18 s
  - output's pubkey: <0290bf1f97ad159ab9e5f0df53db8a0bf77647108f40d44d0dc711c1cc57235f>
  - in tx with hash: <54c1ed5037aff23e836413efbf91bd5c6fd912447c1fc2531850058cb269fac2>
  - this tx pub key: <94c360fd37e928e0ba039d6f32a7bf46b220aedd0bd9becae00ff3fff62cf9db>
  - out_i: 023, g_idx: 916279, xmr: 0.20000000

 - mixin no: 31, block height: 603677, timestamp: 2015-06-12 02:07:03, time_diff: 0 y, 310 d, 16 h, 50 m, 43 s
  - output's pubkey: <3c73be763e9cb1dc237581c7e8f321fd6c5e9d33c69da636d94001f348f13553>
  - in tx with hash: <514738cc67668a57588a274a9c28f4a883083d76f5f291ee1469502b2ac2bc59>
  - this tx pub key: <fffe3829328ca3207054da91eb2f1ec886adb7ba480183ef31ac6c22ebbf2ada>
  - out_i: 023, g_idx: 921569, xmr: 0.20000000

 - mixin no: 32, block height: 617021, timestamp: 2015-06-21 09:31:28, time_diff: 0 y, 301 d, 9 h, 26 m, 18 s
  - output's pubkey: <d318ba1744af083b3228b80f2862319c0766b3b5dd349d49e546b61fe76e8348>
  - in tx with hash: <178fa3207ec72d10a193087e33c62e7ceb23f89c0637095725b39b1361217d43>
  - this tx pub key: <b932109d095158ab195366ba001c83fe4b56345ac634529f038de61ca0711077>
  - out_i: 011, g_idx: 928375, xmr: 0.20000000

 - mixin no: 33, block height: 662482, timestamp: 2015-07-23 04:51:25, time_diff: 0 y, 269 d, 14 h, 6 m, 21 s
  - output's pubkey: <b20bb03ae58a3994d6985954a5a77ed74d71a7063da48752edaac641425d980b>
  - in tx with hash: <dd5d7fa04cb830c0882cb463e34e461d1dbb3ddbe32385ed40164ab51681e75a>
  - this tx pub key: <8e9819f991b77b4977e5e932adec45579204a90b86db10617516a10adca093b9>
  - out_i: 006, g_idx: 949583, xmr: 0.20000000

 - mixin no: 34, block height: 673338, timestamp: 2015-07-30 19:20:59, time_diff: 0 y, 261 d, 23 h, 36 m, 47 s
  - output's pubkey: <290e4312252a024d6b0ec6bdfc4bfa67dd3daff3dd2bb1d1dffdbe0149ea9cf9>
  - in tx with hash: <bbacc59a46635055b24bcd1fd095cf0512f2108c9ff541ffff1952172239e52e>
  - this tx pub key: <8a771cf841f99a5dbe86cefd2bb1e81d524cf052cb0e9ed5381b087e63b2c4a2>
  - out_i: 018, g_idx: 955970, xmr: 0.20000000

 - mixin no: 35, block height: 747915, timestamp: 2015-09-21 02:30:46, time_diff: 0 y, 209 d, 16 h, 27 m, 0 s
  - output's pubkey: <a12a10cebed3ed44afb0d50bab41697b9a604027265e0a01bc2da4a7b607c7f2>
  - in tx with hash: <7bbf35b76bc405ea4bac01d34a48bb87b17f0d7effe4557df2870f2da60287a8>
  - this tx pub key: <7b105a939ef1ee96df3aa234eade5563665a77cce4b92c43eb65789a7b4b9266>
  - out_i: 002, g_idx: 995374, xmr: 0.20000000

 - mixin no: 36, block height: 758774, timestamp: 2015-09-28 18:15:28, time_diff: 0 y, 202 d, 0 h, 42 m, 18 s
  - output's pubkey: <102296efc1213890ca3602a2e18056aa5483695b3a9bf86fce9059a8ad05fc0e>
  - in tx with hash: <35d90b2d09dc1cdc29edd6906e1f93dbd390f6d755325f79850b5ce631db02a3>
  - this tx pub key: <7331e2357703eece3297f4794aed68e66b9750d05fa53d606d33739f89c3b5ba>
  - out_i: 015, g_idx: 998565, xmr: 0.20000000

 - mixin no: 37, block height: 883597, timestamp: 2015-12-25 18:53:00, time_diff: 0 y, 114 d, 0 h, 4 m, 46 s
  - output's pubkey: <4444724e9fa7bf712c5df3edfe998b5e1f4b62f6bcbd9f0d720ddcf5707695a6>
  - in tx with hash: <542e1770da7c481c71e0d73624d295ff7397ec7e2f3cab0526e163c63cbb7ec2>
  - this tx pub key: <e3e0478a81d1c5bd2994cc9e33f3091bf8e730945ee504f8980fb0920e6ac15d>
  - out_i: 018, g_idx: 1050693, xmr: 0.20000000

 - mixin no: 38, block height: 953962, timestamp: 2016-02-13 13:55:24, time_diff: 0 y, 64 d, 5 h, 2 m, 22 s
  - output's pubkey: <75392e820b358563e8573baa0f60a38a42bf3c3cb163d9729bea107d13453832>
  - in tx with hash: <25e363869de22c7db54930a9781eb77bb67b86ec103f51457b781efa60543dbe>
  - this tx pub key: <3f37fc589d2d497f352ba34bf91e3fcda4f6c881436ff375b2cc45afc3267711>
  - out_i: 002, g_idx: 1083047, xmr: 0.20000000

 - mixin no: 39, block height: 1012956, timestamp: 2016-03-28 05:35:21, time_diff: 0 y, 20 d, 13 h, 22 m, 25 s
  - output's pubkey: <0c0ca874f8937c342a6bb2fed5598755d5c9b286192157102408bf9fcb55ac32>
  - in tx with hash: <4367048082fb1f56a7a4b2af591fd9540ecec36d9a502d33302ead3fd55ab641>
  - this tx pub key: <879708712c5dc95abdbaf06f4a79097e021eb5e60c03f3a0ad1fadb107450f84>
  - out_i: 009, g_idx: 1100294, xmr: 0.20000000

 - mixin no: 40, block height: 1014467, timestamp: 2016-03-30 10:18:37, time_diff: 0 y, 18 d, 8 h, 39 m, 9 s
  - output's pubkey: <00961582ef69b5e054c1995c23011ec9468b543ff86033347efb7598a4dfe19a>
  - in tx with hash: <d8ca46c3abbbf25bfd95a087ea3c45ca935c37a88948b40cdd4746ac31a6ce69>
  - this tx pub key: <09aeed2b42e53b785e9820a7ee913d1567e03653c91fc6d9fd596ddc6c73a287>
  - out_i: 011, g_idx: 1101019, xmr: 0.20000000

 - mixin no: 41, block height: 1026806, timestamp: 2016-04-16 17:57:17, time_diff: 0 y, 1 d, 1 h, 0 m, 29 s
  - output's pubkey: <f64597bb81e8782b6a1e2ae7d7de1fa445df019757a903c590b3c3803593284e>
  - in tx with hash: <4f75ebb511f9f6038049a05dd6f328a7b1a7f6a728d8c569b446ae887834ef28>
  - this tx pub key: <8f2973766ca42c9e42a3f897da2304e8576d250e03087dd170259dd8b8afe53b>
  - out_i: 001, g_idx: 1110588, xmr: 0.20000000

Ring signature for the above input, i.e.,: key image <5d55aa0ed8ea271992163bcc88bdd3935e8a3d5631bf3d737754a28762ef816e>, xmr: 0.20000000:

 - c: <9984c43462dda339d2add8d7ee95b1379c798f146e589dc06e906e122a025003> r: <54f09c277a716c57864fcb09fb365f0408b7bfa346177ca3591ed4b5873be90e>
 - c: <f415d9ee10709228c5e21a0a22646d0af864d1e257a4f7af174f8d19a806e90e> r: <1a213be809ced48a55b97e831fb082793d8d15bd7fb84629092c6f425eca4e08>
 - c: <0a496e4636d3c1a78503dbdf74404e1ae8eb7a8f9107ecdd1060d2dbd89a4508> r: <4f29dad02cf533dfefdff918be4a397954c6b58e372294fda01866e7e66db403>
 - c: <5d38d007e5b0b23c766b8661a19fcb2bed9635c99682fca5131e9d0e58650d08> r: <d0dba2be4f7c922bc3c621f6e91ff638b30fdcdba027b761a328f00a5e32be0d>
 - c: <8add22ab50a372f18006e1993171618f195f753415c242f9455e81f73125bf00> r: <6c469d3d47a0ff90a5a74c365ba691d5540a65f7e26ffc39a8762f3b791df908>
 - c: <24e5c2167517a068407d3fae80534e46ba768350d4a878b734c88ebdaa5aab0b> r: <cbace678c07256f4de90a7c58e1b85b3b9cf4c6e931fd82750bcf7eaa135f90b>
 - c: <3cf44b15bacf8ae0f23b658a3fbd6603f966c53e1756c5e3a4c6411fce17e103> r: <c8758b82797bec175d9d135d2b6e2895e2880ca0d0553a96ceab2931f4cbf905>
 - c: <c83e7d80c9b0f2409b0093f56bbfbe745b902f69bf574be43b11cc6759b6a808> r: <c71bc2e1496bb03228201e5d6edd6d7bf6ac8310135f26a485a17b9b45459504>
 - c: <6dac2275404674a24dd83be623c04c54cb002e67d9a06a7beab9a853777e110d> r: <6bd81abdd9c96b5bda04222eff1eb00e3edbbb42b70454abfa47ec4021b3dc0d>
 - c: <a0f5e159bf289a93181d675dea9c98de36e9f693cc5e32d53dbe378063d12c0d> r: <902b29fcf49f5cdd901b7a4b08e23f43c9e59f66b5ef3471aa29a18957086702>
 - c: <7a4208913ad55971effabdf2b8d60947f2c49e0ab00dd32cedca984242c5b107> r: <ac4f1d413dd6e73821ee34f9f14df023cbbea988cda06c68fb089c72c1650407>
 - c: <b640ad476cb544ac55fc746ca9bb3398148c062820eb0b1344704585a3941c06> r: <9668a28881444371d4e34455f55bcf651c1054f51ea200a6ad9612ab8b43360d>
 - c: <577058818a3ee089cbb61f4445824319e3e429b06e64e14fc47e6f6792eb7c03> r: <1b1e394324932cb6f4778512e9a6ad534a2d7d64985f944e7c2146c0b6b5a807>
 - c: <761df6896db316b71bfb1e30b1a60d2ffcb6db749e5fac61063414faee496d0d> r: <5c5a2d38ad242b3e957c5609fb545f175f2a5ea7124bf159cb75c9e8a755460f>
 - c: <dab47b6673c77fe6bc2ca7f6f81859cef6f39c5a21e034f8583a946ef4e2920f> r: <d2607237104d9972f98951f7bffc85624489a49390512db53604ceedf675b206>
 - c: <e7e389fb5083f3ccad9d403f22f3c1428002ca4e4ecf8fc33e6ab743d25e880f> r: <6ef180301cbcde08c0c771ec0ed5fae00b1d124a0d241dfe86935495dffb1c0a>
 - c: <1695e336213dc296396cfdccdb3fc4b6141873fa6fb319dc2a850aef94e37c02> r: <13767251b3db2270199673a559e8810c17a39144384d1f5128bd8926e0166f0d>
 - c: <44d73e4c89cf471ccedc664b29bf127bf1e2f3321b6c3ed42312681ec5add401> r: <da03b909d1b35474d95b12c0740d6d7092edb5133927f796df8f8a58242ecd0f>
 - c: <d91902514853f8b461c514153e76d04c58d9d65f0b3a3ed20e9304fac0780407> r: <cdf7d2a0fee87025cec6d29347a2dc53f8a19467207cf788224d393015523c03>
 - c: <60f2fae222baf7e48e2a76f42740632512f970bd2724b787cec0a2919cb5d601> r: <dc8e2595d0ca7304c5241b58a89535f147e5dfe38642d755acd85780a4d55106>
 - c: <599fa7e8ca21fb768549e0954b515e395033dcabbed31469836f144ab96c7802> r: <1ba1fad4287b35f9a5fe1cf10173a5eb480124bada1cc11aa003eb1a92c02504>
 - c: <93680f3c8a043306ca55f6d590b68e63123822cfb1febaa877c6fdb62d77530c> r: <1c01165a4f96fea490171a37c52c30dcb1d4d05ba96eb999b413a2e158874b0f>
 - c: <3bfd18ce8d5600dd5af2661e9d1cc51d1c14d8cbaa56ac297fada6817c730e0f> r: <f1aa7aaddf5ee1a0bc3db0169b0114d26a1916dee09e9f017933e82d5089cc0f>
 - c: <8de304b810da70a08fd460f4abd22fcac0042d2e9f11b972b11c7d6784d79c0e> r: <413d468cae885e41fb09667ace8d1d6453f6e12f34a35ab2ef821cf22436690c>
 - c: <2ccd23293ef04ff9d86b611390cadd9a863167d006db36d7b3d17854154fa30a> r: <14e1263bf6b482bee1d95dcfcb55ff63ff20c554dce660c157ca43f67b2d6d00>
 - c: <7d14ab25d98c2c3c3dadc673875640b78c8aa0d06ef251c16220901289e53007> r: <5163242441c19f49ded09039c5dbe188eacf775dd9f933ebcb92195f3640f108>
 - c: <38c866d239e564ef29260022564339eb67573280a1befc021e3f14faac099e0c> r: <09e782071d3319d85b9e3a0451de6047590b769dc05a5b2eec159f7048d29f0a>
 - c: <f539f46fdae35d62da58d46fe632c4d7f1a666a53c9cfc5ed513b31354cfcd09> r: <8b557e7a3c4fd2d3c87c0386f4247ac9ccd91a0cce5c191a8cf47c40a4afae0d>
 - c: <4ed8f283eba531e22563879b5cd24cbd97d0d1be377a92764a96bd4a87777902> r: <528c9bc80d343dac10992d23771e9ac24f686136f31b5678738ea325e443af08>
 - c: <b4355257b374d0d5d8b6c9916c29662bd920e8eae57550aba3b2f9c572441b04> r: <65e6738de87bcf691dadc3fcbab661150fa1ddb6d9402a7a7e25e8d1f6b4d208>
 - c: <a564a694587a1ca79332f225184821c4d8d0438f87320a9c48c23386f1e88d0a> r: <d21d9ef347d9ddd2ef7952710ee573f388ce315cbfb057d886a47d05bb033009>
 - c: <3f5c780705a90ca150df8013343fc8563b7aa973a85b4d3b3675479e9582d60b> r: <9d0c5fd9adeb5849181da425c91e82ada3cba9a11eb0e9219e62063e178a4e0c>
 - c: <01d55bca462078fe95ae7d9ea6fb29c7dba8ad6b187912bc728b079bf137550a> r: <c0965390983c1f4efe091b8e0200232b1026b14116a04be670501c6b83078a09>
 - c: <447068616f5daf8874b02845ad82492a5bf969fa21e25e28f807ddd429758d01> r: <b4150338c7e92aa7a063b0b4c343a148eaad7e7f2bf9032685fb9ef17accb402>
 - c: <1c12bf7a6b1e657d14b24603bf753f8d51bac76198c17e70c7298d4ae63c2704> r: <c0f9df381d0473ba4b3884949fdc37ad04fd16367485ad7060adbb452c75260f>
 - c: <ba0d85ecbc34440b897ab59e24628087f58a1f532c834b4fbbd47c3e4b681a05> r: <a838bbb1251b19340fbd8aeb101a197be2c86f128a657f716a2ca737d8311506>
 - c: <c4e9e4d5fe0c7304dc7fc8c00fcc03eabda7338637973a7c6efde8c7db069807> r: <f8d1fe0358cb5bf7259b823ac46b58409b991b5ff2fde470fe6dd0ac51bf3e07>
 - c: <34c4eebb85ad249fe704417c236f0b5b5f867d3d28bf6a5ceb26895320d74c05> r: <09ca2bea77626ad892ff39d2ee14f321766c5d46c1803793cd2e40d2ecb6b600>
 - c: <3698f2719650653f379bb9e1ee0b766ae8064fdb29ef7df8d5e824f4ed3c8308> r: <560e7624875e37bc56c6f9d8321a4e76d3f5c95385c829603e5456bc50069f0c>
 - c: <72cd6b5f941533d138d7fd32db93e91fd6f2c289ca637c82caf234899174420a> r: <a4fc32db7c0e9cf818ad6a4f9a7f6d0af085049bf1f5d637f747648ad3224c08>
 - c: <6bbfc0d39982f1bd1f93a2d827f2ff9a6cdaaf9346a749ff0138eb24b0aa5600> r: <f5bfaf161b3ad7affe831ee8a4dda9e18268da3ef0250d4374c1b2cc040fb10a>

Input's key image: <3388fa17ba0e6ebe447a020edcd7f8aa3344ebeb927d327e224873cdd5ac6d8a>, xmr: 4.00000000

 - mixin no: 1, block height: 89380, timestamp: 2014-06-17 23:07:32, time_diff: 1 y, 304 d, 19 h, 50 m, 14 s
  - output's pubkey: <3dd03b683d9c7794559be751f88bb6899a1d8cd921f8694796a70c586900c492>
  - in tx with hash: <0e90ce87bd13027200eb89165fc139a8f6a7b83f005bc44a0bc46f8686f722f7>
  - this tx pub key: <28a390a786e6847bec8cd671154e84bc3223964ad4e113b41ba6d69f1c5e4471>
  - out_i: 038, g_idx: 9933, xmr: 4.00000000

 - mixin no: 2, block height: 116515, timestamp: 2014-07-06 14:19:34, time_diff: 1 y, 286 d, 4 h, 38 m, 12 s
  - output's pubkey: <72635e93a181d0b8c48501be1298eeb4c331b5ba0bc1cc8ab7b0f7c450b42ed3>
  - in tx with hash: <a86bafd4fac5e0d7f108b011ce5dd2ebcbba791982f0a6fb587e0b3f499883ad>
  - this tx pub key: <668e2eda1c0cb269d05bc5388b7f1badcb585546eac330ffccf3e09300ea8b95>
  - out_i: 057, g_idx: 19360, xmr: 4.00000000

 - mixin no: 3, block height: 171186, timestamp: 2014-08-13 14:33:29, time_diff: 1 y, 248 d, 4 h, 24 m, 17 s
  - output's pubkey: <ccdf1e7f9d25cebb484e8609ca76d845bb44fc125ab55e961ba0972656993e67>
  - in tx with hash: <ca111fcde208c96e60813a6210ef553bf1f304dcdcbc64872183537ab8b91b70>
  - this tx pub key: <e3ae60352008081a844716b8b26c2bd2bc82b987e40d5da6a36612b0c9898e1f>
  - out_i: 004, g_idx: 37510, xmr: 4.00000000

 - mixin no: 4, block height: 181943, timestamp: 2014-08-21 03:04:09, time_diff: 1 y, 240 d, 15 h, 53 m, 37 s
  - output's pubkey: <ffff8fdd0caf8efe6e42c00f84f73a017346dae510bab59bdd23ce9f85655c0a>
  - in tx with hash: <6e704d563335290492d37011b6dfd9379fcf356e6a4493bd7bb16a843524580c>
  - this tx pub key: <3b4374dd27231a38ffe52446ace13bae40509de7ffe030840fcc1cad0d37136d>
  - out_i: 003, g_idx: 50524, xmr: 4.00000000

 - mixin no: 5, block height: 187900, timestamp: 2014-08-25 05:01:03, time_diff: 1 y, 236 d, 13 h, 56 m, 43 s
  - output's pubkey: <df0d3703e6ae3d3cc307ec8be4e263dbb78dc83528b01b0e42c3c2df91baff6a>
  - in tx with hash: <3235e3bd99f1da7b32e724b4485bad9e8da93cae665c550f7080df3a7bf88271>
  - this tx pub key: <91e3d26e18ff9b3611f6428690464823e97c017c3706f278bce4202631d1317d>
  - out_i: 002, g_idx: 57695, xmr: 4.00000000

 - mixin no: 6, block height: 189631, timestamp: 2014-08-26 10:48:04, time_diff: 1 y, 235 d, 8 h, 9 m, 42 s
  - output's pubkey: <42cd34db4970efc03e5070a431eeb55394ef9ace39c6324b6f3d4453fc4a320a>
  - in tx with hash: <e0c59a330223d4b3a8630975c6042043bb403f8cf9078408c20f360ce8ac2362>
  - this tx pub key: <a805a52b4d840b3a72b94100bd7d7012be2e62f0db837e7d0e11a301e361b9e0>
  - out_i: 002, g_idx: 59658, xmr: 4.00000000

 - mixin no: 7, block height: 194712, timestamp: 2014-08-29 22:42:43, time_diff: 1 y, 231 d, 20 h, 15 m, 3 s
  - output's pubkey: <ef09e15c209726ac3531e827f12244d1d8f2c002256bdd6277ae4ac41a725bae>
  - in tx with hash: <001e5cdff9c57b751a6e3e282c9c5a9acda0f76f31c2040af84ec1624e80a73c>
  - this tx pub key: <99401eea2c015379ed8aee2379fa19763b585a7c333cf8e805735c8239e63924>
  - out_i: 009, g_idx: 65537, xmr: 4.00000000

 - mixin no: 8, block height: 199738, timestamp: 2014-09-02 12:17:13, time_diff: 1 y, 228 d, 6 h, 40 m, 33 s
  - output's pubkey: <e8542fe36984054feeec5f9d50060edba1a1a007577b322c7e5c3879551321c5>
  - in tx with hash: <6a000df04b5f9552c3b526cb606d77ecbea642a1b3cd7618fdefb16717184072>
  - this tx pub key: <15c690cace2127e2694969df2c57b0d7ecdad0c6712b4012e9ade249421c2f0e>
  - out_i: 002, g_idx: 71252, xmr: 4.00000000

 - mixin no: 9, block height: 200801, timestamp: 2014-09-03 05:24:35, time_diff: 1 y, 227 d, 13 h, 33 m, 11 s
  - output's pubkey: <4c791376f1a7feae9c8d59f71daa6e82fdab42130b770889e7aa7c16676d2b38>
  - in tx with hash: <dd02473ce55436312447b143e87c59f2245d52d9db48ef8081bf8240d7127c04>
  - this tx pub key: <16340e6db0705ccb4fb8ea84b6a3333a08e5402aba59dc49f55db7d22e8c2396>
  - out_i: 002, g_idx: 72459, xmr: 4.00000000

 - mixin no: 10, block height: 213038, timestamp: 2014-09-11 17:54:44, time_diff: 1 y, 219 d, 1 h, 3 m, 2 s
  - output's pubkey: <0bbd5091db11c0da06144592e449043417b4e87e05ce817331995c556435f62a>
  - in tx with hash: <204d2ec86ac1c37f817b20cd72a1eaeeb692ff766f16b9ba6a83a2f6a4f791a7>
  - this tx pub key: <9d50e592b019dfef2b9d8a6bba485ca76c6269329272dd3738e8ee91ed233d30>
  - out_i: 002, g_idx: 86680, xmr: 4.00000000

 - mixin no: 11, block height: 217674, timestamp: 2014-09-14 22:58:02, time_diff: 1 y, 215 d, 19 h, 59 m, 44 s
  - output's pubkey: <fe5c07455a0a26dfb4992b9d09f8d52d3c81bce132bd078812cb0c4677287f66>
  - in tx with hash: <199fd1fcf1a56977e21b3b71b17bbd80a9a4311ba1fad64221b05cf230c19b86>
  - this tx pub key: <2478f945e8b31a14a7ccd0c2c0655f24222f7edfbe0f756ac87fd869323c93b7>
  - out_i: 002, g_idx: 91986, xmr: 4.00000000

 - mixin no: 12, block height: 224804, timestamp: 2014-09-19 23:20:15, time_diff: 1 y, 210 d, 19 h, 37 m, 31 s
  - output's pubkey: <0671941455291c653581e795564502fd0e4d74ef1742041d65e6f831284a4a23>
  - in tx with hash: <41383904471dfaad6599d827d84e6cbd28fc96caf4303ccc4640f1f1a7bade33>
  - this tx pub key: <59de045c00d71dd9e9dacec1565ac8d585b632dddabf8ff88841893fb98f5f43>
  - out_i: 002, g_idx: 100053, xmr: 4.00000000

 - mixin no: 13, block height: 225372, timestamp: 2014-09-20 09:21:24, time_diff: 1 y, 210 d, 9 h, 36 m, 22 s
  - output's pubkey: <645fc51e8a39dd457d3b20f90df4e4aac3366cdb54a6054ee65db42f7c249b7e>
  - in tx with hash: <a7c1ec1eae8aa01daa0acdb44e5203f8425f29a5d48dd27c6d2556c3835b03f1>
  - this tx pub key: <9b02abdcc465a6ebb1b5fb5423737668a8d57c34d16a15850ccf31c51ca7ade5>
  - out_i: 002, g_idx: 100677, xmr: 4.00000000

 - mixin no: 14, block height: 228341, timestamp: 2014-09-22 11:51:21, time_diff: 1 y, 208 d, 7 h, 6 m, 25 s
  - output's pubkey: <f43b78e9ee4463842d1dc3daa9c7d9aae4e04230818ae34959b62d2099ff1d08>
  - in tx with hash: <b934427ef9d3c43f5b281d15563968c1f5461b6276fbf14a315c402fcb3fa428>
  - this tx pub key: <6383faa3a21ec4e207f28e573d355ff31fea00efc113bb846c07e1ae77ecfe2c>
  - out_i: 003, g_idx: 103987, xmr: 4.00000000

 - mixin no: 15, block height: 231690, timestamp: 2014-09-24 19:14:37, time_diff: 1 y, 205 d, 23 h, 43 m, 9 s
  - output's pubkey: <ab93fd114f3f325c7a83ba571bd61e001929473f939ef0d81e205626b9c70908>
  - in tx with hash: <b02eed35faa19b7b49a6f0b15af03a0f91a2008239e26e30c5b1292d70622e91>
  - this tx pub key: <bbed9f04fa7fd0d2f20c8e4db67ed48634ee1e8a068f1b10f199f02c74013f61>
  - out_i: 002, g_idx: 107786, xmr: 4.00000000

 - mixin no: 16, block height: 235551, timestamp: 2014-09-27 11:55:44, time_diff: 1 y, 203 d, 7 h, 2 m, 2 s
  - output's pubkey: <5e63aa5d15d5a05338f253533def543231c47f018a1517f7210e49f4f55b3df8>
  - in tx with hash: <efaa79d3383a52f86950cb8088c4e4cefc1b0e590906a4894d51d7c077000321>
  - this tx pub key: <c0b67c2cf7b38e269e77e383f48a42a8238c7ecd5048b4dfb3e7d6b2dfad7854>
  - out_i: 001, g_idx: 112162, xmr: 4.00000000

 - mixin no: 17, block height: 236860, timestamp: 2014-09-28 10:42:32, time_diff: 1 y, 202 d, 8 h, 15 m, 14 s
  - output's pubkey: <aad66784f322a0d8e6eae0f4b71cadb23570c48f1045130fa858f6eba76894ee>
  - in tx with hash: <2bf1235d965cfa593304e5da5f3f860645baa80a701748e000979e55aa66434e>
  - this tx pub key: <7467ad45d794c1d4c1c80c98b9636156d2ef1dec8ab5d77aa289a195b1735d1e>
  - out_i: 001, g_idx: 113638, xmr: 4.00000000

 - mixin no: 18, block height: 241797, timestamp: 2014-10-01 19:32:12, time_diff: 1 y, 198 d, 23 h, 25 m, 34 s
  - output's pubkey: <61a245e02483babac579d6ad8c3e34ab9290a5563fab2c53b044558d129f898e>
  - in tx with hash: <307d140a030a7566f4fbdcb495230703b7d89229f19b9e7c207083c38c1772d7>
  - this tx pub key: <c5238b91dfd9785f2d4e3e6d69c61d734bc1675cc20eec240b692d1e01ab714c>
  - out_i: 001, g_idx: 119279, xmr: 4.00000000

 - mixin no: 19, block height: 257080, timestamp: 2014-10-12 13:00:20, time_diff: 1 y, 188 d, 5 h, 57 m, 26 s
  - output's pubkey: <07b9b9ed07c1a53c29b5ec6ae8d9d54398306c82cbd2682cb2f2705dcfbc7e63>
  - in tx with hash: <499ba7bac86724e7adcc065bdcaa751301c53ad1166095a6f61030413f532ea0>
  - this tx pub key: <33b30fc68bdf1ac1b620558672f158894125db4f4e3eb7292b68f0b93a8a6e36>
  - out_i: 052, g_idx: 124385, xmr: 4.00000000

 - mixin no: 20, block height: 304396, timestamp: 2014-11-14 17:02:52, time_diff: 1 y, 155 d, 1 h, 54 m, 54 s
  - output's pubkey: <dfa7f88ade275fae60ec4092b5850c9e016ad35a77c8c9468b3bc1215eb258f6>
  - in tx with hash: <408b2d0c5b38523280c3e753f8d21b6391da00b42941c4f45d9b97fa03371d9a>
  - this tx pub key: <9ac9a65fbd7f6c40c7c135df245aac3909ce3ae411d0eba46ceb76b2debfa05a>
  - out_i: 003, g_idx: 130969, xmr: 4.00000000

 - mixin no: 21, block height: 313669, timestamp: 2014-11-21 05:38:13, time_diff: 1 y, 148 d, 13 h, 19 m, 33 s
  - output's pubkey: <a120a5b6b6dd5d120bf5a56258cf9b5d639e466761034577a70dabd9c3573fb8>
  - in tx with hash: <089510fe8c9d6f50f83aff077f287af68b4e85b07d7f3d07136df3f39b7fec9a>
  - this tx pub key: <ed8ae391477bc37513214ce4608e046977aeef772190c7c43fd49deed3fce7af>
  - out_i: 002, g_idx: 132318, xmr: 4.00000000

 - mixin no: 22, block height: 391362, timestamp: 2015-01-14 18:06:38, time_diff: 1 y, 94 d, 0 h, 51 m, 8 s
  - output's pubkey: <5a92d75b3b844141a17027702e1a3d32c7ff0675cd84f6d5331aca0cbc032bce>
  - in tx with hash: <af9d876dd747202d47a78d4f4f2da3d6c02b4453a34fdba9be839a2d7b49d048>
  - this tx pub key: <f6fa4c39a373fe1035b33a0363ceb2dcf5718b32531961008eff31dbcc1d5f20>
  - out_i: 002, g_idx: 142426, xmr: 4.00000000

 - mixin no: 23, block height: 485170, timestamp: 2015-03-21 05:21:42, time_diff: 1 y, 28 d, 13 h, 36 m, 4 s
  - output's pubkey: <e16faf9ca86eed06fcc96c66494277029cc5b0c8996193ada271fa9edc9b9abe>
  - in tx with hash: <787f92edd42c5d9e00eba73f6035e1e4019487a65f89f5178fe9a511f369e65f>
  - this tx pub key: <cf5fae774162dc6285d78abfd76fa4bc10f6c76f276a4d5850277df8e6f9a2af>
  - out_i: 002, g_idx: 152235, xmr: 4.00000000

 - mixin no: 24, block height: 545981, timestamp: 2015-05-02 17:30:06, time_diff: 0 y, 351 d, 1 h, 27 m, 40 s
  - output's pubkey: <83a70450a3d98ae5b478ee7abcd940f597248365ab407888078f4417d7d2f56b>
  - in tx with hash: <c407fca214be5ba16737c6528efa8e531e00e68c76abcb64f504cf51d5d31d1d>
  - this tx pub key: <f48b94ac349862785122bfff24c89b296b1bb46a22099879a83773dc323cb2af>
  - out_i: 051, g_idx: 158844, xmr: 4.00000000

 - mixin no: 25, block height: 553781, timestamp: 2015-05-08 04:00:17, time_diff: 0 y, 345 d, 14 h, 57 m, 29 s
  - output's pubkey: <8d26116a9af1385f4ea243886185155636377815ec502fd688cbcacc2b0e83ea>
  - in tx with hash: <e4d707b172a517bdf535c6a1de74c2244c22ade7f73adbe031b70d8466d1d542>
  - this tx pub key: <92ab485afc4989eb55dd53a3fb5d06f6bec62ba948633a970d3a4805f6d08964>
  - out_i: 003, g_idx: 159578, xmr: 4.00000000

 - mixin no: 26, block height: 555396, timestamp: 2015-05-09 07:47:28, time_diff: 0 y, 344 d, 11 h, 10 m, 18 s
  - output's pubkey: <72200f2a8dc213fff53a723b3aca605bc6b40de6ff4df4af5ad2a5e2b7c5a570>
  - in tx with hash: <38d25f7c5c02e52144fe3b7b63d46858a8de8faa5273069eb90f508066989ea9>
  - this tx pub key: <26afaf0cd52387a1cb9b4162d57bf6b0dd56972d062ea972ca85d937e4a17f74>
  - out_i: 001, g_idx: 159752, xmr: 4.00000000

 - mixin no: 27, block height: 562276, timestamp: 2015-05-14 02:25:49, time_diff: 0 y, 339 d, 16 h, 31 m, 57 s
  - output's pubkey: <66810c2b814ad66578c96a222531b89698336266539b55f85578f36469c715c2>
  - in tx with hash: <1f418022c54d116dd4413ab84150a81b49f7e98e7d0314eac8bab10b2220c996>
  - this tx pub key: <63133763a43c062f5f44f4716ff384526c52295ed2ec164f22f06becbdf996cb>
  - out_i: 050, g_idx: 160379, xmr: 4.00000000

 - mixin no: 28, block height: 645793, timestamp: 2015-07-11 14:11:50, time_diff: 0 y, 281 d, 4 h, 45 m, 56 s
  - output's pubkey: <79c42883620fbe605f044f8a7b84be622bea80ed578a6dde6210a3b8d984f15e>
  - in tx with hash: <a956eb062e8363528d05f1164524470e5b1269d8b12f6cf00674b5c19e4e26cb>
  - this tx pub key: <9e7e1076804b7b1c51855443fbea0a962808081fb9efac3a06a19cd3195b4095>
  - out_i: 054, g_idx: 167733, xmr: 4.00000000

 - mixin no: 29, block height: 688370, timestamp: 2015-08-10 09:02:27, time_diff: 0 y, 251 d, 9 h, 55 m, 19 s
  - output's pubkey: <470a8fcbbdd85956b2b3fa8b98f80b35b709a3a2d04e7638e5a0758c5c817e17>
  - in tx with hash: <f9c4fdb937110fa33e07ad9e07c5d4adebf05f6ee782655bca6e7bd69e4b86b6>
  - this tx pub key: <10f14cfb44d728300b16b53b47b5b8edcd5d66e71a13cb993ef7ffdde227200f>
  - out_i: 053, g_idx: 170999, xmr: 4.00000000

 - mixin no: 30, block height: 705096, timestamp: 2015-08-22 00:42:46, time_diff: 0 y, 239 d, 18 h, 15 m, 0 s
  - output's pubkey: <5bfc548298bfe0642455c3102b907d5df4bdd86efafcc5485e454938b1d3002b>
  - in tx with hash: <1db2b85f66a6a24cb8b8269e2cf8cb5cb322646eb7a77e4e21f4fb92a79849d7>
  - this tx pub key: <835a620a59dd9a571e06ff8301e016119a3c5a640d8ea4439114be5437684e1a>
  - out_i: 101, g_idx: 172162, xmr: 4.00000000

 - mixin no: 31, block height: 718347, timestamp: 2015-08-31 09:12:35, time_diff: 0 y, 230 d, 9 h, 45 m, 11 s
  - output's pubkey: <c7a2376ffc3a163df6131fe957660e1a1b64cc2c2b602641d186e20543606b66>
  - in tx with hash: <99e0786d9e3fba80f5fc1e29e7eee2f1c17d15b92d3195eee3da3b15913c714a>
  - this tx pub key: <0d8b30372b6824c26699277c08bab5eecd00628a7f9b2e66c70b60ac3d8e0266>
  - out_i: 013, g_idx: 173024, xmr: 4.00000000

 - mixin no: 32, block height: 730823, timestamp: 2015-09-09 01:57:01, time_diff: 0 y, 221 d, 17 h, 0 m, 45 s
  - output's pubkey: <28e856aaf2064be9f02bc04286dd2e7bf70db26d83047db88dd05c79313eedb5>
  - in tx with hash: <d5e486264f7095e7b67bc9b7088bb2024d59a0fdf06fa0f4b551cb45b19bd258>
  - this tx pub key: <b3566b2bc6d98fbe45b40db59f28455a2700ea540a01e30fef2ea36979cd575f>
  - out_i: 003, g_idx: 173869, xmr: 4.00000000

 - mixin no: 33, block height: 772373, timestamp: 2015-10-08 07:40:26, time_diff: 0 y, 192 d, 11 h, 17 m, 20 s
  - output's pubkey: <1ce79ba7dcd1bc4abca5de8d06085b1779bab6098c87a07ee70489efbcc6781b>
  - in tx with hash: <77f68dc114fd31ab05767e4019d3994042dae5830d2c48d2bc740f4c5ca12ff1>
  - this tx pub key: <58c9a61eaed347ab0f24f189e546da605121ccca393acdf3e5021f9d76c57c3d>
  - out_i: 003, g_idx: 176462, xmr: 4.00000000

 - mixin no: 34, block height: 773216, timestamp: 2015-10-08 21:07:39, time_diff: 0 y, 191 d, 21 h, 50 m, 7 s
  - output's pubkey: <a6152c8d4a1e6696781e84b470f504930c207cfefcc62297a907925d49594421>
  - in tx with hash: <a7c24ada37ffabe6c8ad35e4ed3bc5f9454cc07c38e6615e54f833b3b96f491f>
  - this tx pub key: <c28e290417c5a3d698a510a31a456a10b21c55506a8f22fc2529c1ca964f1882>
  - out_i: 041, g_idx: 176508, xmr: 4.00000000

 - mixin no: 35, block height: 801354, timestamp: 2015-10-28 16:24:57, time_diff: 0 y, 172 d, 2 h, 32 m, 49 s
  - output's pubkey: <353d3c08f371186a733ce1f35ad7e0b88be7fdd7ab8e065a2af9af30bf2c1fb6>
  - in tx with hash: <c022a9718f10e15b6129bce1447f51791378602b82fd3e95a85b2c6c829e4914>
  - this tx pub key: <26fbc6a621ff55845e3ba7d8a89bcff94554c3bd49a3c566353ba405f5e3e680>
  - out_i: 037, g_idx: 178317, xmr: 4.00000000

 - mixin no: 36, block height: 913621, timestamp: 2016-01-16 00:57:10, time_diff: 0 y, 92 d, 18 h, 0 m, 36 s
  - output's pubkey: <4f0e0cdf759563581a08163f9a13b79e6658115af75b4605c5473528916a9b9e>
  - in tx with hash: <a120768c2fe80276b4b99f23c0dba7073689654eae6c6ede455b74f84e5d60cf>
  - this tx pub key: <758a6e92886d58fa1fc577ff7271ad0f1e903db249cb4ba54c0b629cd403f635>
  - out_i: 118, g_idx: 186026, xmr: 4.00000000

 - mixin no: 37, block height: 918755, timestamp: 2016-01-19 15:57:47, time_diff: 0 y, 89 d, 2 h, 59 m, 59 s
  - output's pubkey: <e2da80104457930fddb2a9495c35e7da3397c1b9db77a7e1fcd04d8b9df60696>
  - in tx with hash: <88239cea4d5d317e9d8b335ac8f6484ebcc31012ddda0304d371236705113715>
  - this tx pub key: <cd23be9c387303cdedf18468746a587f74704bc9c3e9999c4f50ac80fadf8feb>
  - out_i: 097, g_idx: 186338, xmr: 4.00000000

 - mixin no: 38, block height: 920884, timestamp: 2016-01-21 02:47:55, time_diff: 0 y, 87 d, 16 h, 9 m, 51 s
  - output's pubkey: <25b03e44bb42b28bd8b03b54930b871a500ce315d9a2dd371c01d973091f720a>
  - in tx with hash: <0690cea080238c267c6698aafe6c7602a15a93b6a417516ce8d6646a05ba6302>
  - this tx pub key: <63727b84a129dbade21d74d4f612c1a90fc8dc0f8159410a872e0c51d283d814>
  - out_i: 002, g_idx: 186505, xmr: 4.00000000

 - mixin no: 39, block height: 950117, timestamp: 2016-02-10 19:22:25, time_diff: 0 y, 66 d, 23 h, 35 m, 21 s
  - output's pubkey: <d7c278ddea10a156c6b775f27d7652cdd28e889417d652079b965ae98b0346a2>
  - in tx with hash: <0c9c229fcdfd07003289044116a30140144b5c50f60f06c0d79216ed17a04054>
  - this tx pub key: <d5801978841b434e28191c30d12b3fd68af46214ec8df2943e43104b04727c1e>
  - out_i: 007, g_idx: 188540, xmr: 4.00000000

 - mixin no: 40, block height: 982099, timestamp: 2016-03-04 11:28:38, time_diff: 0 y, 44 d, 7 h, 29 m, 8 s
  - output's pubkey: <91195aee03efe73e7c83f7adfeae5727853bca2015d973ad4483f5de02542033>
  - in tx with hash: <5b4bd0034b15a81ffc1f2160f825cd12f219f0b5cbaba03df06a1107941ed98b>
  - this tx pub key: <fa5a76b9a51691c2f842b51d18a253426d21b9b93d2e0d8e57edcd5b8ba95f1d>
  - out_i: 002, g_idx: 190805, xmr: 4.00000000

 - mixin no: 41, block height: 1026460, timestamp: 2016-04-16 04:11:19, time_diff: 0 y, 1 d, 14 h, 46 m, 27 s
  - output's pubkey: <95394e6c055843df4f148e882138feec6f4e7c2d30eb5dc545e85a2abfcdc070>
  - in tx with hash: <4fca96e538c0bb89b463722b059ca606713432f2dd0e8580cccfb2a17c406f06>
  - this tx pub key: <ea7d7e915fa9cb7064ec2b93fc441342c920f6526f7be71a37ec44876849e439>
  - out_i: 002, g_idx: 195515, xmr: 4.00000000

Ring signature for the above input, i.e.,: key image <3388fa17ba0e6ebe447a020edcd7f8aa3344ebeb927d327e224873cdd5ac6d8a>, xmr: 4.00000000:

 - c: <47832886902bdaa5f267f7627733330daa6aca58646794a476f857136334e003> r: <981e4fc408b5cac4e3e0780c35c4921fa050ac42e9e87ff05690566d8255ab09>
 - c: <1680b46d7f3494bbfebbe3188962f479c83e69e69002d9e58dd894b2815bef0d> r: <502bda4de58fdbb24e33c34785f93cad305a2c929d27103af686fcd81ba81c0e>
 - c: <9c1386599c455a5eca95cde9b24a32df7796915dde9ea514a7fb78421490050f> r: <14bbc5448fb29b39a086055fad253fa311b29982d93af0687f508d079492d80a>
 - c: <b698755b9a9643f0165c401a425b21b1b306156b82b53a432afb8604d6f34901> r: <38e13e788c127093500668e70e2d2d5b1e7c2eef6f9989eccddff02cfd24c704>
 - c: <9be5562161a53f88d1fa7f951437a32484e7c74c51b58fc288c8f999d8e8280f> r: <08f09014508576d90e5286242a395ca74e12bea897caa53d2bed1c2a01a2220f>
 - c: <bb8b5969dffde062efd3a89b44d13576345cdb9e12c69429eec69e199b6cc201> r: <ddfb3553986bc628db2ee3824e4269421837128daebb7645aeee6b299d1bd409>
 - c: <5b159eff9e2e6bf8db5868c95daeeb8a8e9ee726eb5dd449c869de14f540aa0d> r: <4084342576412d9b273fe93b99b186678cd40615d37c94a0c6482204e7fa0e06>
 - c: <805ea0b51273b5a65ec5eb92c3854d9814cbbd61b312faf2d63ae72e4255c608> r: <c6ef8b945e3b6d844eb2504b8f242a592756941079d17ebe4ac5b513c2e21202>
 - c: <b72de2be253fd1eaf797f615a9b01ffe8f4589db0406568daee78c02eca2cb03> r: <811090d74522077c2a404000ef7894c43cddc547f713fec9fceaab7c18bca903>
 - c: <729cfccfd04a7cad265624ba6c49d2ca741721b58f43ac532896cd23e350ee04> r: <0c76edaa493a990a28d05d392905fcea9bad5a5b522ffc395ab798590131070d>
 - c: <5298cc029421f38e668d7d9f1a76cdafeed2f656b186231bfd089bbb82c1a105> r: <ae60293c8907c1b6dd78801447e60958b84403ce32a378a247432b87e0f29b0d>
 - c: <9cdc01268fe3c74a808edcb511b5fcea1ea040c3945c94945b1c0b39ced0d908> r: <7e36dc4cbf60599a4a625a049afc303eeefdc77dd464582abbe494e9573e7505>
 - c: <8a2001d376247125c1525dc3d4c5e3a31482eb6b3ea7cb077f10045afa26e902> r: <03a1a0243bb978b05a712507acb4662d933e59b1595ff5c6492a3f6ae3935707>
 - c: <94a65507e54729f14d3913e324405748798079d45979b3196026cea17df3460e> r: <64b6b2a55ff3bdb0be45c95c8c49d1b1f7955e6885879fea1a34aaba9e9c4c01>
 - c: <01bbce4bc582a824a5e7b396532c1fd276b896f4bcf81d97690b98142903840a> r: <532d61a41a1462bd91bf691d686068638c9d21bb1d4f43eb3d1b4488e32e5c05>
 - c: <535cfec259556dd71dd0a77331acb1c7c0d1f6ace792be44be468e8d2cddf808> r: <e274dd3d5680a9cd409275524867900add12863ab8d669e2752a2ee42b6d6409>
 - c: <4266ab74ee3c456e10e04104e0e383cb35f79c730d25fbb3a295c538000e1a09> r: <79026b50b5ead4f3a6a1701dad83363858ba16eb78fdb6b182892eb5f8a56400>
 - c: <8dde34ae6dbf639cb7373664aa03f084313a0532dc5a607bf17ef00e24d69c07> r: <6d59a6ba590f0b91fe11b5459109cddec440f4e1c96215bec0dee568f81eac04>
 - c: <f3906454eda20175e31994a01a00c6dde939851faee0dcdc447e6c9852fa610d> r: <ce3dcf2c573c6c7d6b79359bdca914fb3a0fb7a872bfae3bbeb3b60e5f73810f>
 - c: <a63625631b21f07140d456570d41e8cff72714e5c7dc21ba6dbc8f93a5445a09> r: <47f4b8afc844cfeafa1c4cc3ebf9c09ce1c9756f462b2b58e38ed88e4d351905>
 - c: <f1bb22e7087437495a5c9d9116f29044248991f8d285c2e3aea4f9cded35cf02> r: <6a0bf0bc2efa74c0756d034a26efa7353b4e9021647c4614d2179f5bc4f6e903>
 - c: <04fa49a97b4d16e820c4d50f98bafd466535f4a9d568d7ca4f272f8bcbee4508> r: <55ed8ce9efe02cc6bce8d9e7de35539ef55acf22937fbf3a7db0dcea83117708>
 - c: <33419b67ac54daf48e8e96045815361579b6c9e711dcd286cb0b416c8724a90d> r: <07109d3e88af522583613dc3d1160f1a755fad4c1bc4b969dd0ccbb2e9034007>
 - c: <b6d2e4fbb53a80c72676f911bd42fd1201cc0dd8e977dddc473e3a0c93547603> r: <4c188bd3eb896c0f3cebb9fe31cc790dc9a40bef9518c88b7168a9f31443cf01>
 - c: <cf079e13712f069fb26faa36331335f9621a78a5be1b880f40fef7565f2f0701> r: <c13fd1fe9e6950e134c07bb8bbda287e547005a6209b1c7747ecf35ebb4b9908>
 - c: <aba1566f4efc73c8ee2e7c9572124bb1f2624b6b3ecc3f4003f7cbffdbc6ac02> r: <fcb0b246fcbb03214e1281981e699a72af77055d2c2f4250d61d13369dd57701>
 - c: <491d0a77fb0ca3c6f33a3a218dba1c45e6daf263b5ecaec0f3a28e3f96f06a0b> r: <929b4ad46470d2d6025ce336b72aee2807eadfbca853347f53a4f0897a68eb0c>
 - c: <879c3a4542442f8649012d404b77fc8f1bcd730cd74f11d668ced22a7ef82d08> r: <522e6c83f35d2eccc9623b4827c170ff995c9f29263bed1f214f641d5d33d208>
 - c: <77547ad5e3886ac79d52f019aa6d75738bb142412fd2c12628cf0506ff029c09> r: <c72fbd57fc9f051e8162e2e8abeb0d109b2b3874ef7a0dcacd33936420070000>
 - c: <6b47c8a9b224cf687bff641ca3709b105e5a2a2b1651d257cedb2470bbc90407> r: <94cee0f240e9fc3da78fa4367e4ca4cafeeca025fb5daec3f8a2aa9ba472d50f>
 - c: <3db588836fbe7a62fb8bd4c0797449cc86b658aa6f9d15ec2f70efc19aa4e704> r: <c85e1a31a83355cb6b29fa5bd7d89eb446bab95cbd4c04655d55c0b46d9ac004>
 - c: <50e87c986f2c9329b8e238477f8a820e46eff4d4cf8ca80386b92503f2d41103> r: <a9647ee35cafee395608cf8e6ce79166ed898966729138006d19ff06faf8710b>
 - c: <98940eebc70567beba971fcbc9e80dfc17297d0a3453638307b3d4391d17190c> r: <9cc932bf0633121f0c9430cd631bb9363771eb7f2137656ce3f63a811c941c06>
 - c: <ef60e78a81d4018d1dc6496572dd9a303bc71663e8c96d8df0852be88256a707> r: <87e01e69b6241f0d8fed5971c692b3b930a698fd2b84ee7147e74dbe213b4d0f>
 - c: <d179fc01794258c2362f41fbb7973b81b6d08e0425775d1f60ef6553885a100c> r: <9a64c9747de2103b2afb2385f5ddf6cd2509021d6d0d16b131632641aa2b9e0b>
 - c: <c6bfa48543ba0db44ca36e4453e1bee8bba6217a95156200b9d2a75ea7696807> r: <02e8bc88a04bae7c11897ebd03af53bb2e7d79e4df8bb9788d310a883e925709>
 - c: <522ff80633f6ecaaf6f9949fdedc070292b1a94c76d7f081efabac0a0976c203> r: <3a9f55b8a827947b0e52fc42dd565935a7e15004c28836d479f40ea8ab4b7b08>
 - c: <4f95e07b07ae7b314c27ddfb33513df6a69bf312a856d5dd6f031b0c05216c0f> r: <4829f20e4d05baafb577c23e0adeb9bab8d0f787dfabe9d27750340045ec2100>
 - c: <be64a3aa2c85f109a55d83ea2edbe8e54f2c98a5be581f31aa13dcb4a41f5604> r: <836fb3e0a1454ff23f4438db36770ea7f4fb04acd82e23707f4cce11c727860f>
 - c: <453b44e9827fed274bcdd52ebdf316810686f8d5fb274377b2434a1cbc7d9906> r: <1d0707a6f4602540e60071e6ab021c1150f30905b3ece0eb75159c826d8c400b>
 - c: <aea5c587a150cc34f5e1d790cbbeb8b161baa462390d0abbaec21761e042600d> r: <427b393e1a12e4f00f55dc1cb9260a4a69a020f7b64801a39769c4aab1306e04>

Input's key image: <3e2e56c989c01e3388d8577388f4ca8a69669db3cdb7893238c6324f26aaad8c>, xmr: 1.00000000

 - mixin no: 1, block height: 131750, timestamp: 2014-07-17 05:33:26, time_diff: 1 y, 275 d, 13 h, 24 m, 20 s
  - output's pubkey: <0ccbfcbced01c6ea068366f404f5813f302d4dafcaca3bafc058a365473bbfd5>
  - in tx with hash: <4433bb0e5f290f7c52d1790d18d51cdf3c192888b79b8895680140ce5e655393>
  - this tx pub key: <7fecf3b1f8b984a4351a79e01c901c12678b6cff766220288fdc5c5dcf256fde>
  - out_i: 018, g_idx: 108502, xmr: 1.00000000

 - mixin no: 2, block height: 156251, timestamp: 2014-08-03 04:07:49, time_diff: 1 y, 258 d, 14 h, 49 m, 57 s
  - output's pubkey: <bb3ffa1d34cc1c5a79a144385b0f595f7ae9cdec5a407ee9a17707a0d069cfbe>
  - in tx with hash: <bbc9b8bb333f7bb7e5aa08d14ef2e8fd460d7928afd7c73fc0d0e49bbd5c14fd>
  - this tx pub key: <843c114ed594c3db5f8ac9d674630b59c64a0d5558db974831c179e087a30900>
  - out_i: 100, g_idx: 160103, xmr: 1.00000000

 - mixin no: 3, block height: 183758, timestamp: 2014-08-22 09:27:27, time_diff: 1 y, 239 d, 9 h, 30 m, 19 s
  - output's pubkey: <e94afb5acde4a893b55bb899bc6937645439df2efdfe76f7698c156f5c65c0a4>
  - in tx with hash: <853fefa03a73a038e45aec6164de392e6a35f727c70c85b8530fedf8003fc3ee>
  - this tx pub key: <e84f241a5a3a06c9ddc1caaf6ce1f8291167baa86f72d1b8649e22e98ee5e52f>
  - out_i: 046, g_idx: 207097, xmr: 1.00000000

 - mixin no: 4, block height: 201923, timestamp: 2014-09-03 23:37:01, time_diff: 1 y, 226 d, 19 h, 20 m, 45 s
  - output's pubkey: <12775c85913989361fd860ebb9c762ea76247752a12e4b6c02d9fd4ff81d0c72>
  - in tx with hash: <abc8af5eb4d0013ed90b7f687ac186aa88d93480455eb7fa7f9866f0cd261853>
  - this tx pub key: <898409aa37012cad93d4265ec19aa84110d2df7f953034dcd0dd6c037098763d>
  - out_i: 054, g_idx: 235432, xmr: 1.00000000

 - mixin no: 5, block height: 220508, timestamp: 2014-09-16 23:00:08, time_diff: 1 y, 213 d, 19 h, 57 m, 38 s
  - output's pubkey: <954b7c3cbc2c0559358c0f913783429a680ecd0294ba90e6d46fea14641d0b18>
  - in tx with hash: <57b493afe08be5895da3c251efabfa9b9392bfbeeb4a7b17d3cc70626a3fdc72>
  - this tx pub key: <de20c4046bcfce75eca50bf34886d951f0a749225ad73801707aabbd454a9e15>
  - out_i: 182, g_idx: 262916, xmr: 1.00000000

 - mixin no: 6, block height: 227890, timestamp: 2014-09-22 02:39:47, time_diff: 1 y, 208 d, 16 h, 17 m, 59 s
  - output's pubkey: <c29f15644a082625dbd1545b093c4f9ced686220cc8738e57ddf04deff635ebc>
  - in tx with hash: <0578a0fd5f04d27010e783525b18603a94ea9750a033afc26cde12aa9bba2a07>
  - this tx pub key: <2c4845aca1b39bb2f01371f1278e6e5a940e71eadc82a768b9aca893a5cc3250>
  - out_i: 056, g_idx: 271986, xmr: 1.00000000

 - mixin no: 7, block height: 240819, timestamp: 2014-10-01 03:01:00, time_diff: 1 y, 199 d, 15 h, 56 m, 46 s
  - output's pubkey: <7e9e0879cb8b611ed8a4378026582515c9a083a5c0a706e9b87ace589a10fbe8>
  - in tx with hash: <0514c0c2fd9ed0e1b3823545a57d4e26eb01e8e27b1cd56668af95b4c572cc42>
  - this tx pub key: <5dea74da70b59d1b348c9e27827fd4e3c0b165f2630eeeee638f1f3851af3831>
  - out_i: 112, g_idx: 284870, xmr: 1.00000000

 - mixin no: 8, block height: 247126, timestamp: 2014-10-05 13:36:27, time_diff: 1 y, 195 d, 5 h, 21 m, 19 s
  - output's pubkey: <c6b656d8b31062630202d8103dd6131a77e667f18c97b4114368c1f469e651aa>
  - in tx with hash: <8c27f720e44fa7b36d45839dc9491ffb3432ca9d5c1e730ac0d5061fd876e12a>
  - this tx pub key: <9f04f3e9de911f36501fb7a7c84865cdb892bddac5892b8eddefd49e41bd3a09>
  - out_i: 080, g_idx: 290789, xmr: 1.00000000

 - mixin no: 9, block height: 269027, timestamp: 2014-10-20 21:16:46, time_diff: 1 y, 179 d, 21 h, 41 m, 0 s
  - output's pubkey: <cc2e925831918516cbbd8fdaefaf79bd2d30153afde59facf04fcc298ae43375>
  - in tx with hash: <1b9d09fee64f6b595e3e82e61455288654efe6ef2ab566fc0dba3f42de2cdfa4>
  - this tx pub key: <b56dd4e40d59de08051d25541aa7c849c566457bb0d3867fd293430987165e42>
  - out_i: 205, g_idx: 309803, xmr: 1.00000000

 - mixin no: 10, block height: 301380, timestamp: 2014-11-12 15:33:54, time_diff: 1 y, 157 d, 3 h, 23 m, 52 s
  - output's pubkey: <f768a3c2a60abead8790e5ed45c9ffff18636968532f3fb74170c3de6d9933fa>
  - in tx with hash: <922398b95fa11b4490040bfc8a375e6e26e04dd6de7cee208852e1de238cb0ee>
  - this tx pub key: <4ea00a1dfd44ecae3006b8abd739d9b60f04334f453d3b088b4840257b8bba56>
  - out_i: 155, g_idx: 332935, xmr: 1.00000000

 - mixin no: 11, block height: 380617, timestamp: 2015-01-07 05:37:49, time_diff: 1 y, 101 d, 13 h, 19 m, 57 s
  - output's pubkey: <1f9842466b5eaaab5d29645e8857d095eea325a31d473bc5c8a27247b30e7c59>
  - in tx with hash: <e1ec3573667099ed9d9c4ed2b4c62d98323d584a31974c424c38e5c68f50a33f>
  - this tx pub key: <ada19efa148b76abc030541b353a00875cab5518399e682f6a6014496df17527>
  - out_i: 052, g_idx: 380641, xmr: 1.00000000

 - mixin no: 12, block height: 413182, timestamp: 2015-01-29 23:09:52, time_diff: 1 y, 78 d, 19 h, 47 m, 54 s
  - output's pubkey: <e5e797ceff38b41ae7c3b6e3019975fdb7c7979c8acac46d4fa455636de18955>
  - in tx with hash: <bbaf9e89a401c48bd50288c401b486e973418f7a0e80c3bae6155081430b8bda>
  - this tx pub key: <ff363803fcaf8b67cf948cf98f6e3c3cadaba91c588bbcdc582c0de3365e4523>
  - out_i: 071, g_idx: 403802, xmr: 1.00000000

 - mixin no: 13, block height: 418499, timestamp: 2015-02-02 17:17:07, time_diff: 1 y, 75 d, 1 h, 40 m, 39 s
  - output's pubkey: <7a003a694977acff74cdc3f97eb4119a655ee7a1b984c9b201b88ae619dc9130>
  - in tx with hash: <241698bb767e1175b573690456fe17c19206e0397da4660ae0eea57de14c01e8>
  - this tx pub key: <7026e660b21e083322536d1b0c67624c74f36265206d28b2ba6932250e6800dc>
  - out_i: 002, g_idx: 411305, xmr: 1.00000000

 - mixin no: 14, block height: 451467, timestamp: 2015-02-25 17:00:40, time_diff: 1 y, 52 d, 1 h, 57 m, 6 s
  - output's pubkey: <f020e17c53bba60f0f169717f5c04cbda2b21ca2ab5a016078b8b2d171d82de1>
  - in tx with hash: <ff8685068d07dde1d591c85eb62005ec014fbfc5c45848b71554666a1743fba9>
  - this tx pub key: <8abeece25e3ba1e5eec505cf384bd4cf29478948d13b3e7918cafdce9c20b208>
  - out_i: 050, g_idx: 458057, xmr: 1.00000000

 - mixin no: 15, block height: 470678, timestamp: 2015-03-11 03:03:43, time_diff: 1 y, 38 d, 15 h, 54 m, 3 s
  - output's pubkey: <02124382e12b678854b7dc7d1e962f97c98b0dd9ae6f8a87d9e994fb1cd97928>
  - in tx with hash: <6d6949615f627fca19b86393c525dc942948d9cf80a86134d9ae3f037995ba5b>
  - this tx pub key: <23152fa80da067ab2603ba4ae4db2b6b88080ef0079224480d2eb6ee226f0a71>
  - out_i: 002, g_idx: 485487, xmr: 1.00000000

 - mixin no: 16, block height: 471162, timestamp: 2015-03-11 11:45:56, time_diff: 1 y, 38 d, 7 h, 11 m, 50 s
  - output's pubkey: <f4aa40c5a2e6925181144fe27fd01720f1cab8b00ded6a40757c5637bda39c40>
  - in tx with hash: <ea0fe604745165c60f4e9aa6fb55aeb541dbc30cce6a1f35a193aad26b9fc2ea>
  - this tx pub key: <32a348ab091aa274a4027329bb39719da41e8c157d79931075bda18e5fbe1733>
  - out_i: 054, g_idx: 486191, xmr: 1.00000000

 - mixin no: 17, block height: 480299, timestamp: 2015-03-17 20:08:55, time_diff: 1 y, 31 d, 22 h, 48 m, 51 s
  - output's pubkey: <d6188076f05802c16a5c07c16482e667fbfc046e76ecdb218910553a52da6682>
  - in tx with hash: <c90813e1e37770ca709dc11c4b8a9bfe9af3d717f19fb1ffbfb1909151201992>
  - this tx pub key: <75f0f791cab0f3d93adc600f2296b70d60777ce72d1de47a1f8b0a2fd1fca955>
  - out_i: 003, g_idx: 499780, xmr: 1.00000000

 - mixin no: 18, block height: 490393, timestamp: 2015-03-24 21:23:59, time_diff: 1 y, 24 d, 21 h, 33 m, 47 s
  - output's pubkey: <ba005c247a7c70c667d3e94b6bf104b1df2afdf93edc09a719b7e619243858df>
  - in tx with hash: <3daead2c12dca81ee81f9839b365e073b81791f7ccd495f0022447c6096e86ee>
  - this tx pub key: <edef327aa814dc97050f20b77f2ac31db1a5a094e569ff87247f39904458e501>
  - out_i: 002, g_idx: 515423, xmr: 1.00000000

 - mixin no: 19, block height: 536699, timestamp: 2015-04-26 04:34:55, time_diff: 0 y, 357 d, 14 h, 22 m, 51 s
  - output's pubkey: <859e3e9171198dbc61bf24185f8bed70de0c61f07e1fa3285280e6b62dd8769c>
  - in tx with hash: <794e4a639c3055b0d61a850345f259f6f28a1ca2789b8edfd5f16d469c7b89bc>
  - this tx pub key: <785d1f2a6988698d1ec37bd6f4d0d13c594175320f7a97b84b6f61df69bd9b28>
  - out_i: 033, g_idx: 546497, xmr: 1.00000000

 - mixin no: 20, block height: 536754, timestamp: 2015-04-26 05:36:32, time_diff: 0 y, 357 d, 13 h, 21 m, 14 s
  - output's pubkey: <d74eef32110f98a3a1e826d0aca266ed79b95acfbbd2983bb801f9cb492471ab>
  - in tx with hash: <27fa76dacfb91200d560c3e24f1e05610e368035e455c9e8c779ee9c5b000632>
  - this tx pub key: <6e5916b742fb705380d60caa4b3b0c72c90349bf0c7e5726410bed166b795a4e>
  - out_i: 057, g_idx: 546513, xmr: 1.00000000

 - mixin no: 21, block height: 549718, timestamp: 2015-05-05 08:33:24, time_diff: 0 y, 348 d, 10 h, 24 m, 22 s
  - output's pubkey: <fd2cf60d7d4caeb9e84aa27038746b2515409b19b711f91cfd0a52cc10e77667>
  - in tx with hash: <643cf740ff4890787448abb5895155d32d2d798cc196ec267940a1479bcef192>
  - this tx pub key: <85ff7241bcedeb9b8225e8ce4da757a3a8ff3d4a0bb79536fe8742a6f9bee2ec>
  - out_i: 040, g_idx: 552479, xmr: 1.00000000

 - mixin no: 22, block height: 559455, timestamp: 2015-05-12 03:37:20, time_diff: 0 y, 341 d, 15 h, 20 m, 26 s
  - output's pubkey: <8c62519fa03707d129e4db4c3250088f729d76eed6e2959672c3b48082e621b6>
  - in tx with hash: <61c581cd80ab41c746cf682e860c6ae6659f13ff8c58dfdb045544384cab8056>
  - this tx pub key: <6f7063b3ad603b5296defceb4b505d81772612d10269470ed530466d69443ed9>
  - out_i: 080, g_idx: 556936, xmr: 1.00000000

 - mixin no: 23, block height: 613099, timestamp: 2015-06-18 16:00:48, time_diff: 0 y, 304 d, 2 h, 56 m, 58 s
  - output's pubkey: <ae8a1e3fb1fc2cc9bb04e4de6c5dd324fc73de1775e64b8ddad5ecf446e3490a>
  - in tx with hash: <c127ef80f8fbffdfb2dbceaf0951f3a81d29b23c817852b4ea3101eebb336f1f>
  - this tx pub key: <4459405b4fbc1bab8f6701bfce23e5412b8e3009b6910162f5e4e252a443ce9f>
  - out_i: 002, g_idx: 578548, xmr: 1.00000000

 - mixin no: 24, block height: 633526, timestamp: 2015-07-02 22:55:56, time_diff: 0 y, 289 d, 20 h, 1 m, 50 s
  - output's pubkey: <fd2a172f7b93bb8be8805f86af4d9618c26644b8274ecfd632ee3f8d06ed6f9c>
  - in tx with hash: <f9edece02ebca8e634661de8f37206713f283a0799af585b6cf4347e406852ac>
  - this tx pub key: <8f5eaa40bb15efc17aa293d823e267b959b9baf366a831981e72f6c638f51732>
  - out_i: 002, g_idx: 586488, xmr: 1.00000000

 - mixin no: 25, block height: 662203, timestamp: 2015-07-23 00:30:46, time_diff: 0 y, 269 d, 18 h, 27 m, 0 s
  - output's pubkey: <06ffbe8922f23bd069e8fe7ccd18b03dd8b2083e1524317b795857620c0859b7>
  - in tx with hash: <7bdb8c52ba841345eaac03052ba6c8e2694a6f72d41e9447321da0faf95e913c>
  - this tx pub key: <82a9018a16ddb3e51063aaa91c386dc8532dfead5e73e75d9b787a2dcd1888e1>
  - out_i: 010, g_idx: 597763, xmr: 1.00000000

 - mixin no: 26, block height: 702515, timestamp: 2015-08-20 06:01:31, time_diff: 0 y, 241 d, 12 h, 56 m, 15 s
  - output's pubkey: <e578a3be45090eb8a8e38c5707ee06da17dee355f7f36fcc9136c4075c3cf0de>
  - in tx with hash: <9e992b2ec7a1180eeac8c8d2ad288421f76b951af6bcda213e5f94395defa43e>
  - this tx pub key: <52ecf0e940846c87701bbddac423d8027612ab08d38716641e52961c86a31e3f>
  - out_i: 027, g_idx: 611448, xmr: 1.00000000

 - mixin no: 27, block height: 719781, timestamp: 2015-09-01 09:23:40, time_diff: 0 y, 229 d, 9 h, 34 m, 6 s
  - output's pubkey: <35191eec6362849301cb68484313a4499a21d023076207f5b39542c09e3b31e0>
  - in tx with hash: <417eb6060e25e9feb51684f982e5d8e31086f5896803841fd8f02fa8cdc072a8>
  - this tx pub key: <b3d46e8fba58c26af140df2159ea3a2e8f32d4d9130114dee29e37d4a06be662>
  - out_i: 002, g_idx: 616648, xmr: 1.00000000

 - mixin no: 28, block height: 763751, timestamp: 2015-10-02 05:33:28, time_diff: 0 y, 198 d, 13 h, 24 m, 18 s
  - output's pubkey: <877737a269f545e544948670d81308ea9046c75ee0536640d82aa58a2de6028c>
  - in tx with hash: <25ef8782c0f09edc953fd5d7e36c4dfb11b3dc4690d497b2e6e2a1c1a72ea3d2>
  - this tx pub key: <44e03f52fdba858ecf269230844df5c3e00c8306d4b64c13a5ca257879a11eb3>
  - out_i: 014, g_idx: 630699, xmr: 1.00000000

 - mixin no: 29, block height: 773969, timestamp: 2015-10-09 10:58:27, time_diff: 0 y, 191 d, 7 h, 59 m, 19 s
  - output's pubkey: <6d25c07b583d50a5f13d105ab20d756fe0f5dc8a28aeaf41389b6961fa934008>
  - in tx with hash: <294d7bf5ef16d7f27fcb5e8abae62d281150d89591c24ee648283f820c1431cd>
  - this tx pub key: <ef19b29e524dfe9c1a039f31b35eded504d468cde6bc615b36ac1f957ab15cdd>
  - out_i: 015, g_idx: 634103, xmr: 1.00000000

 - mixin no: 30, block height: 788878, timestamp: 2015-10-19 21:51:20, time_diff: 0 y, 180 d, 21 h, 6 m, 26 s
  - output's pubkey: <c3d32b252d9c178930781991af5bd366b2127a431d099fb5886684d6c151565f>
  - in tx with hash: <a2245c9b9e496e5ac880c84d09307fa04631d3abb3de2c65cc0cfab00c0e174d>
  - this tx pub key: <def3d424c0d8dc89b680ffdcf876b56c2c86ca2fcf4bd5f99f8ef5883b81001f>
  - out_i: 013, g_idx: 638748, xmr: 1.00000000

 - mixin no: 31, block height: 794352, timestamp: 2015-10-23 18:42:59, time_diff: 0 y, 177 d, 0 h, 14 m, 47 s
  - output's pubkey: <640eac841376fe1b3c08c5dec203bda3b76125d0423a2bd369680f74f36d5a5c>
  - in tx with hash: <24fcf01954734b7eac8bdc91ca3181f5d7b57b33acad577e78bc66c5509ed22b>
  - this tx pub key: <05f2617a19211914c19851f73a37613f21a3a18a29b7183f0cbf6cdb6ce88568>
  - out_i: 069, g_idx: 640573, xmr: 1.00000000

 - mixin no: 32, block height: 806998, timestamp: 2015-11-01 16:44:37, time_diff: 0 y, 168 d, 2 h, 13 m, 9 s
  - output's pubkey: <cf64fcf70e651a341ae23064109268a6d52ad88df2a6f4705d5a5cfc9e172558>
  - in tx with hash: <abb0a335fafd4e615e54147bc3c77a449fcd418b9e6513dcd00924b2370d5d74>
  - this tx pub key: <a938da124b514398429e98cd5c020d7f374a15552ff04998a55c34e02be2035b>
  - out_i: 073, g_idx: 644158, xmr: 1.00000000

 - mixin no: 33, block height: 835839, timestamp: 2015-11-21 23:32:21, time_diff: 0 y, 147 d, 19 h, 25 m, 25 s
  - output's pubkey: <dd8a9067f57a0066b5fd8e650c15d2dbbcccd408bb13b73fce2a7cc0a52910dd>
  - in tx with hash: <1ec7b2a5d50238264e6e4e5f2bbc5a4b1bae9e1caa472eab108d7563eecd8bdb>
  - this tx pub key: <8047580dadea3b93367f562fa16ba3fb995392606cf3e0ea28b893a0f0b9356a>
  - out_i: 016, g_idx: 652421, xmr: 1.00000000

 - mixin no: 34, block height: 853613, timestamp: 2015-12-04 14:58:48, time_diff: 0 y, 135 d, 3 h, 58 m, 58 s
  - output's pubkey: <dd8db220e8c07e915b1189df4154892f5aad1e6fbae8bb6e8eee0fe2a81a31af>
  - in tx with hash: <7d2e923ae1fddc9a34155d0a155561e50deb624e1bc9e75a9fe9647ff43223ad>
  - this tx pub key: <20159f4e41bb8d770a83cec7e3303dd160e3ff8990ef999a2965bd5d4c140d6e>
  - out_i: 034, g_idx: 657023, xmr: 1.00000000

 - mixin no: 35, block height: 876878, timestamp: 2015-12-20 23:37:19, time_diff: 0 y, 118 d, 19 h, 20 m, 27 s
  - output's pubkey: <4c1a302c4ef77145243951621fa6841b2b0ec79f30bb5c48b6843dea49f5c2b3>
  - in tx with hash: <1df6c02991a3661e4e3b21f056a662c2b689f383dec307b09a082660668d2bcd>
  - this tx pub key: <a1080e4d047a838013c36e8e47cca42aaaa8b14e7dcc56d46b5be9ca19f4f15c>
  - out_i: 003, g_idx: 663067, xmr: 1.00000000

 - mixin no: 36, block height: 880674, timestamp: 2015-12-23 17:35:31, time_diff: 0 y, 116 d, 1 h, 22 m, 15 s
  - output's pubkey: <4d52f21892c70312f5aa6133e8c21ba8b6875a3efa2c43ed79feb1d0333f5f9d>
  - in tx with hash: <8e6fa6f9ebe1058a9882e901aa1fb97b92cfcc3371dc812e87691018392997ab>
  - this tx pub key: <0b3fa2bd3b97611b964732624e6792067f07692a1a6c0b1d0605c2735181029d>
  - out_i: 008, g_idx: 664081, xmr: 1.00000000

 - mixin no: 37, block height: 881922, timestamp: 2015-12-24 15:30:52, time_diff: 0 y, 115 d, 3 h, 26 m, 54 s
  - output's pubkey: <070c14b1837fa93e756f5a8351874ba064a7f81cab19cb05fd0bb74178c35d48>
  - in tx with hash: <51b914ba41722b3aafa188741ae621a591803960979a8a1a33bf797483569ebf>
  - this tx pub key: <aa37e9dffc38f696d1257f73389affdaccf47da5bbd453832b8d707a4ec05c2d>
  - out_i: 003, g_idx: 664376, xmr: 1.00000000

 - mixin no: 38, block height: 888425, timestamp: 2015-12-29 03:34:34, time_diff: 0 y, 110 d, 15 h, 23 m, 12 s
  - output's pubkey: <53531c26120a4e8b927f984a1e56fa67dc5021881ba8ff6ae6a78806548b9a29>
  - in tx with hash: <aa4a948b72d767f642663c802c2cedfad6c9867d3129f42eeee89a35562b921a>
  - this tx pub key: <1e3a29b81ee5491a7eb642b9072a82b85a564cd836e0768aa94a445d8b7e5328>
  - out_i: 095, g_idx: 665746, xmr: 1.00000000

 - mixin no: 39, block height: 913380, timestamp: 2016-01-15 21:57:12, time_diff: 0 y, 92 d, 21 h, 0 m, 34 s
  - output's pubkey: <c67ba316f8264ea3adb7892f3dc2717682434e27c66c6d4276dfffd10930d34b>
  - in tx with hash: <1a7c9405e39fce4fb6c38aec3d3651dd492c19ceccfced7238aeda19a1cb6332>
  - this tx pub key: <e832041443079b140835d9662b922dde5eab6230ff6a9ed11a7760f9dd94bee9>
  - out_i: 114, g_idx: 671696, xmr: 1.00000000

 - mixin no: 40, block height: 932806, timestamp: 2016-01-29 14:01:51, time_diff: 0 y, 79 d, 4 h, 55 m, 55 s
  - output's pubkey: <93acfc3aa332954558a2dbd87777292ab0f95fa32adb95c718667ed82adb3ce6>
  - in tx with hash: <bf67cd1e41ae0b736657e3c260ca2142bc9dde7b62fab47bf0a22f33845d6446>
  - this tx pub key: <47bbd7b677824a70aeaecb016f5965b6b3ce39aa6fef02649013631db9f860cc>
  - out_i: 018, g_idx: 675848, xmr: 1.00000000

 - mixin no: 41, block height: 1026806, timestamp: 2016-04-16 17:57:17, time_diff: 0 y, 1 d, 1 h, 0 m, 29 s
  - output's pubkey: <ed3ca739e4feed1d5ca9eee17431ea2fd440c4f42d4dd322375171cdb6e6c42c>
  - in tx with hash: <4f75ebb511f9f6038049a05dd6f328a7b1a7f6a728d8c569b446ae887834ef28>
  - this tx pub key: <8f2973766ca42c9e42a3f897da2304e8576d250e03087dd170259dd8b8afe53b>
  - out_i: 002, g_idx: 700796, xmr: 1.00000000

Ring signature for the above input, i.e.,: key image <3e2e56c989c01e3388d8577388f4ca8a69669db3cdb7893238c6324f26aaad8c>, xmr: 1.00000000:

 - c: <1c00e9bdf820f1d3c933b03dd3438b17508e7ab3b25d8edf5d676c067825200d> r: <e730180d758da7fac96974c542e337c15c9b4d379b3edf56791b858c832e0a09>
 - c: <c8478440f334279e1f200a3d386d8a7ecbbca9a0e5dc13b36947df938beb8d0b> r: <39c2f18120658e25175f80fe29b32a87458442549edcfeea0e8d66dccd186206>
 - c: <71968c7605b5b296e782560bdedd687f4412b9b87263774b738cb935988d870d> r: <65e739be5e4916901aa30f24cb68637807d4fe2315a4f37fcd240eef5da9740c>
 - c: <dbe87a44b62123eb1d37ba90eee99cb2b351f628be8c1f772a7dbad68b45de0e> r: <e2199817e0f9dad8f9450c15e71b8f0278f3f28cd33debdc3168c0c9b63bfc05>
 - c: <fd61b444ad4b7476ce34cc22dbb5d6242d27c73f23e5be4d4cfa41ca7e52cf09> r: <ecf9fcc60218e9b966e15b40d69b4e78008ca64d3982b4a2635f90d2a82d890b>
 - c: <6bde245584902b46a7fcc14418525294926d2a4a3389ff3022ba0a1bb38c6505> r: <38adac358402827e104ee07a03e388a6177e8dfeaa699e727bbbc26cad35540d>
 - c: <85c5c6b91e8a7421266f5eb29c3287799f0b200ce12525e16b1d87b171305203> r: <08fdd8d7bd5659a57c59e07588552f68fec27132c0696e18c45f1c97c4563404>
 - c: <fdd9d82a5b1dec2be7d8baa43d4b3ceb6617b33de271d258ad38d1cf09dc2a0e> r: <a8420e85be5735b05014e1ed9cdfe0b8d1281d04b787153fdd127de46b3fb50a>
 - c: <1bfd5703b6f3a5d611d6e97c42132311134acf497031a5935570bbe384e2ad06> r: <23b93e8a8718833c5a3ac7657c2d81aca0e28b707fcbcbc6f503d5120bda860d>
 - c: <64b8567886e562ded19b8f5660217474e662c68ae3eb52e0c7163fb1f3afa609> r: <8a63301446a35260cae608b6f591b8e1cb5804e1d2d3aa41dcd4fc4d7a09c904>
 - c: <4a482edca4d0af41377a39778ef6e9bca920b06d94c9a49a72bde7ea03f04a08> r: <f77a848bd0f36b8f521f0c19da52ff14a54958e5bca0b9d8ccafabbd3ebcd60b>
 - c: <2053af882923b0b2e446bdfe005baa45660544fd48b578f1c2ad63be23bbdf06> r: <d7e329a937f36861ed95cab8711cb02234cf3cbaa1997b5a2f60f254b4db2d0a>
 - c: <4963ed59b730f5b5b515575e29c0e7692c7937a56002d092bb3d549924159508> r: <5dac563aa391213d9f7c17d3409fc11664337702969abe38c303a1922d458409>
 - c: <f6cfba03cb8688eda39297c24db71c1273653af03a82d796201e1618c3890705> r: <ffe3d075130df4d074cfb7987a0fa5556e3f42be39d2edc8267611e686bfc70e>
 - c: <7e4ba9e927f1d8a5596e409783f96b14fbce21ad160dde0c643e0cc1d3e90007> r: <f0fb12d984eca911b6e9f4ad80ea96fe9c4834e0d95fe645e526feda6af2d503>
 - c: <1c64832828968a84969fb3b2cc60bb9749a7d68ad4d99b2a1abc69874c03a60a> r: <cd21a03bccd4ba888352c6d63e1bf8944ecb73d266742d505f9f5e4f7c006c09>
 - c: <b3e3787aad66a674c0c1cde8bb13d6aaf307c578b40ae5dd90af746b556a460d> r: <afdbf728b276ddce7857cc82f1717d582692b1235bc0835d669474e81ce84104>
 - c: <e7e0e07d6527ab4a1efac8a908601f2288f25b6d508ba98a30d44625440d050d> r: <e65d5baef1a356436d6024f92f5266aa2acf88a99ac8e28e141137546c21190b>
 - c: <18761214af14a64bb06dc64f604f31309c0a64ad66b2228063930876f8946601> r: <61c89ae5e5df4d63518d0f65a23ebe9157cf3fa1c2c16d1b166fc5e60bf9bc09>
 - c: <8983cc963985aec795ce8743770f4070db4cda079a7954f02842eb6674497507> r: <b6445fc258b0a719971111b0167af675991ebad6d4b3412ab510afab1a33530a>
 - c: <93d6c150e021508e462c3d96139d434430a5297c8fc47eabd9ba65bc6d852305> r: <dd620f5555595118d55bf054a70ef7d0be8a52da156c7c0250fb5c4cab8f700d>
 - c: <28fd20defc69ac0d046cb08460d60b917a1a8641922019202f827ccd2fadeb07> r: <ec54be949aed8d4cb73e7d3de140a4fd29f6d79c1f0335b91e9266c6d8244407>
 - c: <64cdc09f43ec1b6fb53db0fc4f61895f3674f51b0f9f5a698ebb3b9ff501fb05> r: <92b0796f37f4a0a73125ec5227d4398130cdb325bb8248b6d6ff50a8e70e5608>
 - c: <2a907d28860de0b6c4b354ab4f948304c9cb0bb6d3940c2fec6ca0bd1805d800> r: <c4983ac56237be64c1d4711b8bdeaecb6e5f2ac3e1b8d5aaf91819358d9bbb0a>
 - c: <2699cbabef1076f3e0d3c4c823fe8b0b1716adb0a205372e69a3d3750988640a> r: <43087596802bd8b34e1d1b1ea3cf54a82adf4270a98c964b3e881e3a60893d0d>
 - c: <a2d4b744f1f7464dc5aac74b85bdf97124ae87ebc60af9441f6d66aa1dfecc05> r: <88903ee2664cec9115fa0e6f09adbb2761cbdd41e4c2053b770c9b5153609405>
 - c: <dd1025f9cbb1adeadb4eb392fb2d1b50b0f90a6993a6b8b4ece56398f54e660b> r: <cd1a6536a017d8e130e5e976eeeb0ae53e1fdfa5ae9d0ff5cc37b6a081548901>
 - c: <3b1df7f0b40cd45e3f8a5ad34c8e089e2dad626ed38993671a2dcb65ce187c0a> r: <7f6be9bcb4e7d065a929cc800e1e18081a655ab6824f37be9b3c536e16f5c907>
 - c: <723cfc1395982ec485abc901a8d4d0c0be9952a5d8803ac5883fec2ac3191802> r: <7c6be0118f108a85955a44c14a8b1197ba75ac0e0d7a0d8968814a31a036520c>
 - c: <67a1fefb72d669adcca1f817641fe9f137e5689110daed7059dcf3b18d7b4e0c> r: <1f9c6de2106ae1d548707be395a22f4fcad56f3db9e7f2212941614d3c30e60d>
 - c: <c5b623bcc5267740720129003f9fd4f9c6da9918e22820b0536ecfc35761bd01> r: <49f0810d2b8889763875504c920f3b7139178776cbb351445595ee5d0fdce90e>
 - c: <ce43d2395a12a6c57d89269c69afbfb2f51412804645ee3201bfd0c83e691d0b> r: <9e54163811e6476c1caefa3109c2d3c4c64e714279cc048cd51049692a664f09>
 - c: <c5f9d5722b690b79e7bb166085943204bf790f697c5bffb685d0ba6e70921004> r: <e0fcda392cdad752a72e0efd4a66e61026f820ea7e749708f64a392fcbdfdf0c>
 - c: <21aa9716fecbc79ca0b6c1e043b15b47e1cc4b7703f91ae9e04b458dc5270b09> r: <3abddd7df36904ced3dd064b52b1f604a843368530647421ed18966a6dd5d402>
 - c: <ea49c03cfa6303c935fcdfd21474d038203a4e05898e6b6ad6ebd42fbfe67303> r: <ef2ff0612ca6e0a634ef7224c20136c0c5208a8da6a9d8fae0be6ce94a0ace03>
 - c: <e22c43220d4f2c8db0da4da16f3742ac2eeca1f5e5d1592c757502a080e0c101> r: <93a27fd843c4b5879be52a824cfa302047cef746728a65e2134ac499a876a506>
 - c: <dae04060e4be7fbdc851c727328023c8950eee0a15b4b3d00eab25315d9f650d> r: <3a0464681da2c5a3071e0db4d9c18481a0c20a4fea65874445a1500654d3410c>
 - c: <0b7253b8be184da6874ff49aa100774b391e3a0139e9357b0384e754f635750b> r: <5c62e65803949deda64d09217f3ef0e51628a392e67d1beda04ae9183effda05>
 - c: <635c1758b9e4b47db63c8b0ef8c75f8faad460f2fe2f3a600eee1107e014bf0e> r: <1321ad6f81137b8a0c640ec7f3d8f64d748449642aa05eaf2f2d56d5e3d64e02>
 - c: <d25ca0fe923ed8dd559e0e5ed6b623a4dbcc4ce2d35f7b8eb6122082deb5980f> r: <6fc9afca9c7ce7103fa6c7070190f72761baee2a274a3e95ebfa2ce069312900>
 - c: <db0abc1b15d072abf13d8d6b03274da76892a54e3344a73105765f0297a38407> r: <c1d3176d63ecea4068b7da11d69c1476f7a2f296eec55a2d09cecf97e68f9805>

Input's key image: <8872f70b0bd46cfa78377e1f6cf0f61bd9cca9334f9c92eaa21595308c92e526>, xmr: 80.00000000

 - mixin no: 1, block height: 271754, timestamp: 2014-10-22 19:32:49, time_diff: 1 y, 177 d, 23 h, 24 m, 57 s
  - output's pubkey: <f6057033050d8a2a66f26d36310b11301e525ac99687c7e69716772fa804b227>
  - in tx with hash: <0a1795bafc55c58b53119bfb870e3dfd784cfe563e7e6ec96d25757891f7bf59>
  - this tx pub key: <7da9e0655c294fbe95302c4a20749e0733395d8b435fa7cc3cfdbf9aac358fa7>
  - out_i: 003, g_idx: 2775, xmr: 80.00000000

 - mixin no: 2, block height: 314895, timestamp: 2014-11-22 00:49:49, time_diff: 1 y, 147 d, 18 h, 7 m, 57 s
  - output's pubkey: <6ad775acdfc981a6cca2faa40517406b58cd05adc8b55a3ff0e12556dcda403e>
  - in tx with hash: <e0e821a7491005249839ce185c41b106a69a3c09ea2b9cae7f1f20bb62cb784f>
  - this tx pub key: <d8c4a9f427928516e969c90b198bcbb79af29c090ef064d567a11faf689a159b>
  - out_i: 056, g_idx: 3300, xmr: 80.00000000

 - mixin no: 3, block height: 350033, timestamp: 2014-12-16 17:54:39, time_diff: 1 y, 123 d, 1 h, 3 m, 7 s
  - output's pubkey: <55faa33e74f58a4ded58b3d32599cdfa9c180e8afb2c2afa5ddb88b01f2893d1>
  - in tx with hash: <235a39b7d27dd45dc2ef103abefde48884465a2b6768f40ca8d388cd6dbe06b0>
  - this tx pub key: <43d7721eb9dbf2a7b57f9bb43b735c1e64ac0cc86ba2cd0c4f30141e7881cac9>
  - out_i: 153, g_idx: 3954, xmr: 80.00000000

 - mixin no: 4, block height: 466904, timestamp: 2015-03-08 12:47:16, time_diff: 1 y, 41 d, 6 h, 10 m, 30 s
  - output's pubkey: <05030756e70bd952ca0d5a6b6e465833e025098c1b14163abb0affa3f514e22a>
  - in tx with hash: <a3434ee8271df76c0cb98bdf72b22ed0bc591781259a04f15597723819861657>
  - this tx pub key: <5e76935b1623047aa75cbb38ed2237eb719d2aaf55e26ffb911569c71123062e>
  - out_i: 005, g_idx: 5315, xmr: 80.00000000

 - mixin no: 5, block height: 509961, timestamp: 2015-04-07 13:17:58, time_diff: 1 y, 11 d, 5 h, 39 m, 48 s
  - output's pubkey: <dd708c744b43376ab22d2c519276eddf8e3d1533d254bc9572d670d026dae5ae>
  - in tx with hash: <135b8ce691ed35b02ba3a8fb9b817795bfcbad67246f65e89a80207aa10efdea>
  - this tx pub key: <6046ff13710a646ed4aa80e5f9ca27865dc23974298ebe0cd23c50402c8345bc>
  - out_i: 003, g_idx: 6041, xmr: 80.00000000

 - mixin no: 6, block height: 603109, timestamp: 2015-06-11 17:02:27, time_diff: 0 y, 311 d, 1 h, 55 m, 19 s
  - output's pubkey: <88cbcb6ce94fb68687f44e450b9baa35cbdbd5f452f193196d477cea819f9559>
  - in tx with hash: <e6af30547d725ceb5009b0bcbe9beb6ea6b2013efcd2164c6192d114c5e73be7>
  - this tx pub key: <91e2539aca0ca4c34f05b6e84d3a2c9c18aace6219de95c9ed5c0eccfdb599c4>
  - out_i: 006, g_idx: 7091, xmr: 80.00000000

 - mixin no: 7, block height: 668101, timestamp: 2015-07-27 02:58:15, time_diff: 0 y, 265 d, 15 h, 59 m, 31 s
  - output's pubkey: <ee3f6e9ddde3ca8a163f673d5b2214a838304e70600c0ea79503e374727a56a4>
  - in tx with hash: <f1abd2f16da24a8bb3fb92ea43336747827b95153dc1c9609fc55b9617fce1bc>
  - this tx pub key: <f2ea830ee7f39d9d19c34b5dcdcb9c7f8b551a3cb3109cb84210339aedcec3cc>
  - out_i: 007, g_idx: 7830, xmr: 80.00000000

 - mixin no: 8, block height: 693951, timestamp: 2015-08-14 05:03:23, time_diff: 0 y, 247 d, 13 h, 54 m, 23 s
  - output's pubkey: <8720a06eb753c1c3fb70c4f7115a5e1f29a545429077d2f247acdb01ec839b5f>
  - in tx with hash: <18b3142f5bc6ba47afed4423448c6cf9d2ce303b984878202314810540aceb69>
  - this tx pub key: <49258e686f0fdd1cda6e15e88bd1cbb8ff68697c5c04ed8fdd1ec6af791e5451>
  - out_i: 005, g_idx: 8223, xmr: 80.00000000

 - mixin no: 9, block height: 709909, timestamp: 2015-08-25 11:50:11, time_diff: 0 y, 236 d, 7 h, 7 m, 35 s
  - output's pubkey: <a5b22f93d62ffc4064bd861fc4f17670d0d5d47aea1380e6bd5487745c8d117e>
  - in tx with hash: <5a71b7ac9bbdb51a169d12536bd1167cf4795a6f32e882538ebeadd08979f3b5>
  - this tx pub key: <831c19ed4dd235ba1db743695190588dc2e7dd1fe724e30554c18b2e06d5057f>
  - out_i: 002, g_idx: 8428, xmr: 80.00000000

 - mixin no: 10, block height: 712825, timestamp: 2015-08-27 12:20:53, time_diff: 0 y, 234 d, 6 h, 36 m, 53 s
  - output's pubkey: <5fb18ff2a4acfad8cdbbc5088bfdb0973af6bdccc328e7ca324e606cc185a8e2>
  - in tx with hash: <bc1518ae2fac6ef11c8b31037ff24954e871c1a1785f6191667df166536dfc3b>
  - this tx pub key: <b6d891ab5feed3af67684672a3824719d29ae57603ce4561035d4046b59d0783>
  - out_i: 004, g_idx: 8463, xmr: 80.00000000

 - mixin no: 11, block height: 714624, timestamp: 2015-08-28 18:00:46, time_diff: 0 y, 233 d, 0 h, 57 m, 0 s
  - output's pubkey: <24fd074403d042989a40fdc449bb4035f2aa7cb9a557c650ce26874859ad2618>
  - in tx with hash: <dd5d321550a014e4280759a816e8e8167a74edece47c7b9f06dc096039ab6dec>
  - this tx pub key: <39a62be6f87b8e093546c5392b405a150a800059de0438b620233ba4595f5016>
  - out_i: 009, g_idx: 8485, xmr: 80.00000000

 - mixin no: 12, block height: 737817, timestamp: 2015-09-14 00:01:00, time_diff: 0 y, 216 d, 18 h, 56 m, 46 s
  - output's pubkey: <a5ba32953b784646c228a1b79302d75e6abfa5cba8d6e4137d8c90b33ae43de5>
  - in tx with hash: <de5d2c71af557e8f703a6142aaf579d3de940b6c3c44f34182085d8f9e75128f>
  - this tx pub key: <f28afc6bc1507f1caf1afd0e629e2b8ba3e1ce9906ae7c7ab975f89c314ceedc>
  - out_i: 005, g_idx: 8750, xmr: 80.00000000

 - mixin no: 13, block height: 748185, timestamp: 2015-09-21 07:32:38, time_diff: 0 y, 209 d, 11 h, 25 m, 8 s
  - output's pubkey: <401bbf536d80676fc181580cc45b3d7e935a09c2df1c20bf767c8505c1f2cab5>
  - in tx with hash: <07acb5f9e8a29e48f0341afb0f99c83c73dd7e283ffa6c18e4674552b366b152>
  - this tx pub key: <9b0fe5e0229863064f0712285bff48b4689ad838ba6300eebab870479518695f>
  - out_i: 005, g_idx: 8890, xmr: 80.00000000

 - mixin no: 14, block height: 748810, timestamp: 2015-09-21 18:19:54, time_diff: 0 y, 209 d, 0 h, 37 m, 52 s
  - output's pubkey: <cc2cc16945bb8bba89da4f79fc6bf6b48bbbd1135b4da0bc234e82acb1d2e6c1>
  - in tx with hash: <241d4d0eb28e7193e55689eb3e8000d497f88d811355c8ca2b5df4b23bfc625b>
  - this tx pub key: <abe9f8d8cf3283b36841e460355f28e8687d520d3ddeb44457848074b88ec180>
  - out_i: 004, g_idx: 8900, xmr: 80.00000000

 - mixin no: 15, block height: 759081, timestamp: 2015-09-28 22:27:58, time_diff: 0 y, 201 d, 20 h, 29 m, 48 s
  - output's pubkey: <521bf8ab8993449eca0677b568777dcd0255f1c642a2b7dd95c28e15f7f9d02b>
  - in tx with hash: <111c6e2efd9966151d28e0815400b1efc1730deb8ef5404ac2a9c066d0c29690>
  - this tx pub key: <78f5f51d849b6b92d772c127a2fec2f88c67eb61acd7b43e7fae1ef8b31b01d5>
  - out_i: 126, g_idx: 9043, xmr: 80.00000000

 - mixin no: 16, block height: 773420, timestamp: 2015-10-09 00:05:45, time_diff: 0 y, 191 d, 18 h, 52 m, 1 s
  - output's pubkey: <8cb3510220300d9631600323c0491bbf88bc0154e039fcf4cff3b15c70be8e80>
  - in tx with hash: <0084fa4fbc836cd7c6587a218633b9e02a7c63bd19cd2c2a7fd173e2454dd52d>
  - this tx pub key: <ed4ee1cb81dadaf477688b247bd7cd0bced596967678fc94e90744720bf7548b>
  - out_i: 008, g_idx: 9224, xmr: 80.00000000

 - mixin no: 17, block height: 790807, timestamp: 2015-10-21 07:01:25, time_diff: 0 y, 179 d, 11 h, 56 m, 21 s
  - output's pubkey: <482aaa212724fe3e9f303e233e170334dfb1cbe6f94dc40d98ff844df42aae5b>
  - in tx with hash: <8768a89565957d5c0a502f542815098a0968932dbe69013f1e55957331711ae8>
  - this tx pub key: <e82259c9295f12337e641c3a52830893f8dda7daa9e61edf8a694cf89499235b>
  - out_i: 002, g_idx: 9430, xmr: 80.00000000

 - mixin no: 18, block height: 794026, timestamp: 2015-10-23 13:51:40, time_diff: 0 y, 177 d, 5 h, 6 m, 6 s
  - output's pubkey: <0517c1ea2fb360597218bc955a1357531ab65778dce9d60e57f465cf59192251>
  - in tx with hash: <bb634fc787c10838bc9c5a99861d1210d32ef873ee64e7f693cec8a36cf9fbd7>
  - this tx pub key: <0a81b10dca357e08da5d0418e26f68f2f89bae21c2ca664a8e9766d0c1022ce3>
  - out_i: 050, g_idx: 9481, xmr: 80.00000000

 - mixin no: 19, block height: 811896, timestamp: 2015-11-05 02:40:43, time_diff: 0 y, 164 d, 16 h, 17 m, 3 s
  - output's pubkey: <3fe9218889ee8ccb890de57af91b16be1479842ee6f45ed2035246835e3e7c1d>
  - in tx with hash: <f3ea4de3fb09b6643ea9e45abf9510d3b523729ad2ce41b89b89bc8f326ba79d>
  - this tx pub key: <badae87073edb602c888f2a11f5b42316b71de2f9c1569d6531b9aeae50f2519>
  - out_i: 006, g_idx: 9705, xmr: 80.00000000

 - mixin no: 20, block height: 840099, timestamp: 2015-11-24 23:39:17, time_diff: 0 y, 144 d, 19 h, 18 m, 29 s
  - output's pubkey: <a0117f8b18c0f1f2877bb1d41205e31c0d9f31813c103a3091a2e88e8ec0754f>
  - in tx with hash: <c5e4abc6ff95fa2687a305b382a0769fa7f38261c97da20dd7b9368528e544ef>
  - this tx pub key: <016d37b94d59f5ba96d565048d47a5f10ceb8800c344b2e5810ea8a1655a97cc>
  - out_i: 048, g_idx: 10013, xmr: 80.00000000

 - mixin no: 21, block height: 840474, timestamp: 2015-11-25 06:15:03, time_diff: 0 y, 144 d, 12 h, 42 m, 43 s
  - output's pubkey: <a4d160e4d7c6f76f5568dbc2ea04e00fae92e34f62aa6e44dc98dd11d4f0576f>
  - in tx with hash: <72d6fcc07af83fc84a50032d269db2eaa5d5f4eeb5e930ad18c2f9ca0cfdc43d>
  - this tx pub key: <70d6fb99d4e1fcafa6f15b531200ca464a85ced35be09d1ae666351d7f92d15c>
  - out_i: 008, g_idx: 10017, xmr: 80.00000000

 - mixin no: 22, block height: 845475, timestamp: 2015-11-28 19:19:48, time_diff: 0 y, 140 d, 23 h, 37 m, 58 s
  - output's pubkey: <3d50e43827d325a66e0e7fc1088fc75958e321075f075207d4feffce4472f2fa>
  - in tx with hash: <96dbbab15dc7b104e4a31fa4fcb1b23677fd86b7253699fdd02f0c08ac6d06e7>
  - this tx pub key: <b7ae592ad043de7e71e36c29211fc161a38f4479eba48c71feaa2b9054ab55cd>
  - out_i: 008, g_idx: 10086, xmr: 80.00000000

 - mixin no: 23, block height: 860464, timestamp: 2015-12-09 09:01:12, time_diff: 0 y, 130 d, 9 h, 56 m, 34 s
  - output's pubkey: <6a51e4e2e957e2c611daed50d301cadc131c09645e5e06f1ab42139796ca683e>
  - in tx with hash: <a1d62806415251699909fab5d2c88d03097546169ea6d8734a7cc4add96c82dc>
  - this tx pub key: <31ee6e8b201ea88e0d13a2a9dbbf4fbcb4934e48bea920e0d2b2911b8525cbf5>
  - out_i: 011, g_idx: 10297, xmr: 80.00000000

 - mixin no: 24, block height: 869933, timestamp: 2015-12-16 01:50:32, time_diff: 0 y, 123 d, 17 h, 7 m, 14 s
  - output's pubkey: <5a0edeb3948688fa9825a640a226f38924b6771c4976089fa4b1b3eb51d7cc87>
  - in tx with hash: <2792bbcf6fcc11d0abc3c9cef487d9c16df49a17d9aa67856aa632ded227c976>
  - this tx pub key: <c7c0183678358dfd557213ad33da6a6dfb7679fd0e75600cde83627f46c34383>
  - out_i: 007, g_idx: 10420, xmr: 80.00000000

 - mixin no: 25, block height: 884746, timestamp: 2015-12-26 15:12:57, time_diff: 0 y, 113 d, 3 h, 44 m, 49 s
  - output's pubkey: <edfa3dffcc833d686221af4388e13479f6e6df60b4fb7416e5a32db127fabe5b>
  - in tx with hash: <d82df228477a2c400ecc43e25fdcfd6ab9fcf9b79607358ac0409bf8656d6bd7>
  - this tx pub key: <e9884ce6a0e30db7492238ff67096160fed4dfcd6efe9b7393604c6485b8d8f6>
  - out_i: 049, g_idx: 10590, xmr: 80.00000000

 - mixin no: 26, block height: 906987, timestamp: 2016-01-11 06:36:25, time_diff: 0 y, 97 d, 12 h, 21 m, 21 s
  - output's pubkey: <7dddc9d02d38dff34875bc565b27aed0021c29b7d2e591b1736f9a4c5765143a>
  - in tx with hash: <a1a7dd9011fe91ce1a117f5399eb94ca64328cb0bf12e80007ea991cabb215f1>
  - this tx pub key: <220b77dee19bf576c0e390450456fdd8eafcba10b2aecdd765d6b8df7cbb96ca>
  - out_i: 049, g_idx: 10849, xmr: 80.00000000

 - mixin no: 27, block height: 927875, timestamp: 2016-01-26 00:47:40, time_diff: 0 y, 82 d, 18 h, 10 m, 6 s
  - output's pubkey: <df0da0d5a1b222a3d7f09655d1c275e4f473ad5fcde47a7081b431cbde0984ac>
  - in tx with hash: <488999a4d797bd437b5ae83714f7e117a634711368ab84456c2854a772796835>
  - this tx pub key: <95143ea4eb663dfa510e6d5f17c7de7a9e07a6fe8228d586b720550a93bc9de5>
  - out_i: 005, g_idx: 11153, xmr: 80.00000000

 - mixin no: 28, block height: 932149, timestamp: 2016-01-29 00:26:37, time_diff: 0 y, 79 d, 18 h, 31 m, 9 s
  - output's pubkey: <474abbcd9c0e668591ecaf184d951ed116d8ec064d4637985cb67edf41a11d70>
  - in tx with hash: <2cd6175d8f3972e1f969293e11970aba970b937256cc7e0afdd9605b6b165dbd>
  - this tx pub key: <fb23e519c27c1b7fe045c0d68068c6849c9c889684c4672792dbfd7f689820c8>
  - out_i: 004, g_idx: 11220, xmr: 80.00000000

 - mixin no: 29, block height: 933228, timestamp: 2016-01-29 19:19:51, time_diff: 0 y, 78 d, 23 h, 37 m, 55 s
  - output's pubkey: <0c2415453372ca1afbece7815e733e8ec937ea1e48f1165b9a8cca4c44d9053a>
  - in tx with hash: <c5db47188ea3cd5a0e88ff3b3cdfa559c72241f4f7b8d56f6abfec1bd5cb9b36>
  - this tx pub key: <f5c25d757f49309da585703c3b88c99b8a01f3c4b14aa5b9e29119f89405de18>
  - out_i: 003, g_idx: 11250, xmr: 80.00000000

 - mixin no: 30, block height: 941670, timestamp: 2016-02-04 20:13:14, time_diff: 0 y, 72 d, 22 h, 44 m, 32 s
  - output's pubkey: <e33284b4bc0826fc1d7d1e2db31f6d7700c3d59ca891aeeed37f23d605d54b52>
  - in tx with hash: <ea78f62747f8d47c1f4c14c56563be0db3bd91a9eff1596521dfef36fbdc2a65>
  - this tx pub key: <49d708ffca4fbc8cd2a97d4d1f7f14fdbe32a2fbf9fc5a1d070e80c2d1745ba5>
  - out_i: 054, g_idx: 11360, xmr: 80.00000000

 - mixin no: 31, block height: 950132, timestamp: 2016-02-10 19:34:07, time_diff: 0 y, 66 d, 23 h, 23 m, 39 s
  - output's pubkey: <6f9cc850b369c77d2b75514f8cbe6702136d12436997f64b640547e9ec3a34ab>
  - in tx with hash: <126eef40c6bdad000ec46cecf5a1be8adda2185904505ea6620d84cb0ec0288d>
  - this tx pub key: <f384be1b1f18e0ae54e803a2cb6170c033b8fdfde5681665f8ed0bea7060c916>
  - out_i: 001, g_idx: 11477, xmr: 80.00000000

 - mixin no: 32, block height: 958708, timestamp: 2016-02-16 20:22:05, time_diff: 0 y, 60 d, 22 h, 35 m, 41 s
  - output's pubkey: <ccba094b8d2790f182033940179d67d7652118004a9291215be5b1603ee2745b>
  - in tx with hash: <dfb3659d9ec0a72458c1bc16099c2dcaa58f1bb9d3f9565f853df8922ecd3825>
  - this tx pub key: <68ebe6d226fd950014c976ad05dd565010a75ae3d60272bef72d93eb7da7e55b>
  - out_i: 016, g_idx: 11641, xmr: 80.00000000

 - mixin no: 33, block height: 959199, timestamp: 2016-02-17 04:23:19, time_diff: 0 y, 60 d, 14 h, 34 m, 27 s
  - output's pubkey: <eeb32d6816268f807687284bb90f945ee8c18e4ebc74ef01424791edb59c546e>
  - in tx with hash: <60f40d4667d71b2ac0d8d4106ab81f7f7140ebaabeb46a9b86325c5ee7573980>
  - this tx pub key: <8cf7d621676feb0557bb4c7cf4d5e13d4a84796133e2902c04911857e027e39e>
  - out_i: 010, g_idx: 11663, xmr: 80.00000000

 - mixin no: 34, block height: 978352, timestamp: 2016-03-01 19:09:09, time_diff: 0 y, 46 d, 23 h, 48 m, 37 s
  - output's pubkey: <a45fc5cafcc0bbc751cf2deefb5c2e4005ec75381fbcb04ee2f80ae0b12f7ecc>
  - in tx with hash: <659f3f1e94e5cfe43b5305dba3ca4690063da4545f5e721166b6741aaa5435e1>
  - this tx pub key: <87e493ecb62da4a3cc3c3eb7006d15dd2d3545a84d1b33cc539488f98b5930ba>
  - out_i: 050, g_idx: 11974, xmr: 80.00000000

 - mixin no: 35, block height: 980378, timestamp: 2016-03-03 05:07:48, time_diff: 0 y, 45 d, 13 h, 49 m, 58 s
  - output's pubkey: <780b8b7bf6cc060af015411a9b2b0586a84b71fa5952dff626caf5d7148f29d5>
  - in tx with hash: <fc26e7c5470315975ae1da9b3765aff664e9c1d3b3c9b82772c5e9490e42a512>
  - this tx pub key: <dfaeb665fc7838de809cc5cebaf9993a452c00a2be7a2525bec38a2ea9ec6ce2>
  - out_i: 006, g_idx: 11997, xmr: 80.00000000

 - mixin no: 36, block height: 985546, timestamp: 2016-03-06 20:25:01, time_diff: 0 y, 41 d, 22 h, 32 m, 45 s
  - output's pubkey: <ea9c87afbc546b427a6921a3abd82e810ea97008b7f10f94edd1fb06f0ad1add>
  - in tx with hash: <38ad540b490b75f2b88101e2df21d51062f60093764ef144ae6843b6ecd606d5>
  - this tx pub key: <cb425d4420ea1f064d4232884a3066c3109eb34aee9650bbc09bb8c53cb8fdbf>
  - out_i: 003, g_idx: 12153, xmr: 80.00000000

 - mixin no: 37, block height: 986104, timestamp: 2016-03-07 05:59:47, time_diff: 0 y, 41 d, 12 h, 57 m, 59 s
  - output's pubkey: <1f1900a4ee73024c39ed128d9e3caa37c60cfa5d4261755f33baac55492e059d>
  - in tx with hash: <5a5ca3b9baefdd8636a8a6c57fe142c3779b14fbda3fb7ecbcfae067f01d9e79>
  - this tx pub key: <c2a453bd9ec934d0661e5ed0ec30c88400cdddb1759a6b41f3f36d27af2913cd>
  - out_i: 011, g_idx: 12161, xmr: 80.00000000

 - mixin no: 38, block height: 1001801, timestamp: 2016-03-18 08:00:55, time_diff: 0 y, 30 d, 10 h, 56 m, 51 s
  - output's pubkey: <c970a070ed196f46faaab3112613c0db838f6b81454ef9cd2fc69c051218e977>
  - in tx with hash: <716b869bc80119abe4613e440b3403dbf452d5bfdad37782a4d7a52bba2df4ab>
  - this tx pub key: <3f0c6a265632e0186422fb203bf1e3c85abe4796af5b76818a1bc07866babd27>
  - out_i: 003, g_idx: 12453, xmr: 80.00000000

 - mixin no: 39, block height: 1011009, timestamp: 2016-03-25 16:35:12, time_diff: 0 y, 23 d, 2 h, 22 m, 34 s
  - output's pubkey: <4807e843e5936896fa428b92b397490fe5520c4fad93907db30ea61f922f5318>
  - in tx with hash: <8e83feb84363bbaed6b4548947c289764d6bcf512770ac4a909b3f3a0588097e>
  - this tx pub key: <570df1855aa3860bc2acedfec3382bd38719e24371b9d7be8e5ca894cf2dab6d>
  - out_i: 015, g_idx: 12730, xmr: 80.00000000

 - mixin no: 40, block height: 1013334, timestamp: 2016-03-28 19:18:39, time_diff: 0 y, 19 d, 23 h, 39 m, 7 s
  - output's pubkey: <3978ed5e3f7bcb41f72ca6d3f0984b12db421795456dae325d5dacce9da6616d>
  - in tx with hash: <70ccbb6fb85bcd25398e98dcbba69655617a1490b4e86ceace1b37400695a07a>
  - this tx pub key: <d0d006df7a4bed36bc481a72852379342101df42e0c245427253712ec3f5c29d>
  - out_i: 037, g_idx: 12817, xmr: 80.00000000

 - mixin no: 41, block height: 1026806, timestamp: 2016-04-16 17:57:17, time_diff: 0 y, 1 d, 1 h, 0 m, 29 s
  - output's pubkey: <c93b127c39545c880cd751014afa937b0503e4fd41377ef0b0b98a2582547682>
  - in tx with hash: <4f75ebb511f9f6038049a05dd6f328a7b1a7f6a728d8c569b446ae887834ef28>
  - this tx pub key: <8f2973766ca42c9e42a3f897da2304e8576d250e03087dd170259dd8b8afe53b>
  - out_i: 003, g_idx: 13163, xmr: 80.00000000

Ring signature for the above input, i.e.,: key image <8872f70b0bd46cfa78377e1f6cf0f61bd9cca9334f9c92eaa21595308c92e526>, xmr: 80.00000000:

 - c: <48a4bcf9affb7bb5643ce7984b2446b8eb6b98c9cbe84a888f5d4311d9f8e30a> r: <bd4fb2966966ec5d1f9c6dafe877ef8d764d4ff3e77422efcae9a2e466b8e50f>
 - c: <c447f03368c6222a96ef5e3e8fbf18499e7354db65056a40fe769681e026930c> r: <b95082fef4fb61cb13a6026031952515a9b03da48ab7635f9c5a2072aad64105>
 - c: <8ff645e846a7406ae07fd24cd9b54a21e049050a68271d3a0cb4667a9be51e07> r: <0a95828dbefc072ba6eaec66f9739b960b4ab4e706f229720394e650f304af07>
 - c: <bfba730979dde988f0047386daf7364e144e7a43e060afe5796ab1ac6d3fac00> r: <be6f6e64b0187381041b88e048be6fc4dbdd991f347ef65a3aa108e29766dc0c>
 - c: <a3b512eed7d5720a5c1e1f52c8515b1d48fbec3cdd6cd0350a9985635ae23002> r: <6724d800df4b15c52678a6f82126e023241a645505316f925928654c79bf210c>
 - c: <cd2bf7828a42aff457c2c63c56f7d4987ada8943816f340b37958f6a5e01cc08> r: <48b25a5a48bf277a0135d627e1d492cad47cbbbb0455b30942f29cea7799bc02>
 - c: <bcac0ee8b4ec78bef77644fd4817c17b47be5d60d26f291ff2fe33e988c9ef0f> r: <22a011ef90b3d06bb142993b465f8e715e6d429eed5b72e70637a349d03ca509>
 - c: <4f1e70ebaad20c7c1940fb119d2dc8f6067dff3022ec1b12bf5facfc350d3b01> r: <04f36060a09ae26845c3b0fbf90a17c452dc5693dcaca925c51093d98956520a>
 - c: <7be9dc4b38093d501fa1fea3e1ada00e8cde71692e7f14b074e8dd4be117b705> r: <42ac0c5e30aabe4961b590af30a56a6566b6793318727b0aa9ef35f89d2b7309>
 - c: <2aba9ee4a9ea3688fde9f60c32994a7cd6ba90358de41299cfdb3cff29521005> r: <285d2e4b6070a51de03d65ba778680ae35ffe48d5551175b3361d448407bf30e>
 - c: <5fb1b2369019167386c37b7615de0dcb9df793a6c47e1d9474af8803e0b78f01> r: <0ef7089d14aef286369a1d107daa6cb384d871fd204baba72bea1846a4816202>
 - c: <6a30841eaabf03fa538fefd0c4efca3355d08634ae9762db1449e4008de3190b> r: <e35db63d48744e47178643b74c360b55b766e501c4e864b03d3642cc9de91509>
 - c: <850c438c7a74d5dac80a85fb42427ce33379de4095077380cedb7c13eaa22d02> r: <5517d9da817f546dae60029d56cfbe667c1f16700d6d597b61029f07ab240a0e>
 - c: <12b527166571745f4aa1a03b5c023843b8a5917437cf614a73c9103f7513df02> r: <269d0d2240c7f29ee072017fa99f8c8cddc600004b7035865c73845cd511960a>
 - c: <ba5e0480bc79d02902e4c6c078f5dd20f391f2ad2e86f65b0c2886c2d841a304> r: <8fab822c18088407aeb60b9004e52746aaab6eb4d52ee38982bdd2ffb903db0b>
 - c: <1fd32c5e505720c914b34796581e37d66b58a4822447e151b809ab5e7d45c109> r: <67752eeed982621919cea846049eb2814e1c31db8c424557d6d1d4a5995f1a07>
 - c: <eab7d1adf2dc8bff94b91769a7f483ad4fe1cbddee2306a132d326a50931e008> r: <7e3f52e81ef9f23b10ef26260e3d564281fa7f701d2ad15f49fef9e5d32e580e>
 - c: <d50908304668df97c8f0ad562790507037209f494b0a00ed59799bc0b46bd90c> r: <166d498b08397d79a8aa73908008fd56f8e8dd781713e45ec33dfa43f8de6806>
 - c: <2b7d71b4b92fb981e7da59e41f3b5b5ac651867c5c6f7044fb04f6ee8b240a0d> r: <afbc48eb1c88d4394931ae501ffa5e1fb716842f5122079841267f7addfde80c>
 - c: <ea73ec26585600686fefe380d6d6ec2cd05f9799f0d2341d6ed6cfffe5671704> r: <d0251871c07be6bde5fa3ce06f1aa90637ebf156adbc608db64b5e0a2b65520c>
 - c: <89017009f230fe2dabf4f914072a90321fe5b361ca73967676ffabbf8d988601> r: <7c4bd71d11c3a24efdf50551a929409d21a748be3ed90fc941e45f13d55e7003>
 - c: <f669ed61ede6700552490275481db160ab200168b8fa3bddf262c89ae32b0d0e> r: <e4c6ecc6de07e31d68d20d28d17bf72bef5a915730153c01564c0f204c0dde06>
 - c: <45918a00024821ffa99824af9f9e9da5ab30f5e3b5481e063ee7cc8a79419b0e> r: <d645d39ac7a7da632bdc0a616f1f7e7b4415b1982971aeffbff3d2d830040209>
 - c: <25e3fb2454106e3bb5c6f3a8238f01dfc32836318078dc306257ed444aec3907> r: <6dde4b899126c667c5410ef465e1de7a5641044c829c22d12b6deab641476008>
 - c: <152586f0c19689a6be33fece2f7e9846900ea4069dac4c47a19495eb70ee8e01> r: <0c341f2535e89fc3b826c229525f7fdf7d13f8a3a3862b78aa6b2c545c1e9a04>
 - c: <891fe450ee50656267ec679159b42d549a4733ca6ba0e63fa9fb67b50e06e005> r: <6e79c1c3b5063aa33bebfe1e2993b1e62f0e00ac9609d2ddf2c75997e5d4cc01>
 - c: <5936761092197fc9ca422144e9dac29fc4d3993d15beb3348f62897684bf3408> r: <35ad93015f6bb8a23a657e9a4022b39ba62d27376915bcad0062e15e1910b902>
 - c: <f80241cd2a32c5b6524d96319fbde85c2809f398179dfb2a3b792824b35a5807> r: <060e5c4ea1e8cf84d2a767b31491bd7abcb2900a755f4bcf27ea29c289854000>
 - c: <1caa719b621e1a26a2e7d0f3e1943cf2c07567923981fc38d77a61e3c602de06> r: <6210d6ea92d011ea5916aa7017d7c9e05bd6b5bf278c8d8fa80650b835b6c10e>
 - c: <3b7f04e20eed4dd73cd1d874e0d8359f963b707885b3eb9678dbf0eaf2ea530a> r: <8bb3789238c7440b8f707414f244ceacb9c34a2e489d4e3830f67d682df41e02>
 - c: <e5e175c1ecb9ef17cb5ce35990991a79066455978b442ed60f1ad4753a70e40e> r: <f71f48a65cf538732aa55e3010a950cce0e5c03b9d33b22812ef5eb8748d630e>
 - c: <2e95e9e88e7895a2c435eab45763449ac8d4c7b5f11ed74be74ed21f1f5b9c0b> r: <f17a372cce731aa328816a459ab819afc37380d642d624885a7be4b26de39c0d>
 - c: <a1eb52ca953cc4273cbee58dd99d5808fa0bdae09c4ded4c52d20f9534fdb605> r: <07e781c06ddfb72f77c867b834b5b47f2bb0a1995d3759e1529fe0fbd3d3fd0b>
 - c: <3ae00124e672ebe06bd5a3147eaaa040cb4281e49a09ad75ec0d71ffee37ab00> r: <cbed20a5c2562b78749fe19a0d6e46637868de461b9d81ccdbf0c8a120d7500e>
 - c: <949ac6520a2c69173b12b721679c5c30d706af73c57b30c28cd8f592e68c2604> r: <2b8b86d1e2471e1507994a7587ba7a4022b738a9aa704cd2232327435b78ec09>
 - c: <a0928bb98b33cbc0ed7b2fca93e7c7584c2a7e2f8ecb0b88a25491c72fdaee00> r: <a6ab29c0973d8cbbe51b561259c182b4970a5c1bb911b92a75accad4543ba301>
 - c: <a11b791bf5cbad350c1b3ad7733feef847940baa5ec4908c1318258a6045f60e> r: <f8ceea1839b6348c36c0d5a8687e7c4f6c58e9870679ba40a5e85e30606f5106>
 - c: <cd72ae245d48d27631b515312408f4186bedd420cc2e418338c88281d31f0102> r: <a3a67dbc5fdc97e60e5fdd010d2ff8c2086added5ad90f4e019c489b7ae96201>
 - c: <4ae73dec28b0508c84088eb2f953d10c30b889a91e6626e95699dc50a9b24207> r: <1695fc91672a9ec4fa26dfbd6942d9c9fde7d4c1d76d287ee47f3375daee9207>
 - c: <b03f70bc45fd4830908d8532e08e1e7976605a5c61e27302d3ea79904e310f03> r: <437efca5df5eb7aa0267cf0b434876ea1afaba4fcd4235e5cbf36eca58b9c305>
 - c: <b8fdab1960be5d0d5d2e8c6458cb508bf27cbee6a216434245742ef051394907> r: <64ab1ecbaec5a2ec49d285da341009087bdd3cf45076aafea6391a30d10bfe03>

Input's key image: <035f873ec6bf28ca4456adf72a7e60663b775b52aee580cfb33862cc1a73d24c>, xmr: 400.00000000

 - mixin no: 1, block height: 115898, timestamp: 2014-07-06 04:02:47, time_diff: 1 y, 286 d, 14 h, 54 m, 59 s
  - output's pubkey: <3ad90bfa34b16079ba561e6f482c9f66b186530db0401c7c12bbfb4f038ead2c>
  - in tx with hash: <d5d1345b6e1c70ec5444e332fb387701c92f7c1f4d8b6e4434329d70576de1b9>
  - this tx pub key: <590023e8ac38efd1ff76c5fc7f54b7e7b6c7d43877284d7eea7c92146eb553bd>
  - out_i: 009, g_idx: 518, xmr: 400.00000000

 - mixin no: 2, block height: 232215, timestamp: 2014-09-25 04:39:15, time_diff: 1 y, 205 d, 14 h, 18 m, 31 s
  - output's pubkey: <b34ba4927aef8a5821c844612279c70f7f1c60a75f01ab7ab6550ba6f0f62c6e>
  - in tx with hash: <dc501db5c830391a973e65dc53776aeda5612a1a2e24c47dfdcafa2581dd6bb2>
  - this tx pub key: <083fa7e4455f507fc2c12f4ae035bbf2d2037cc2f06bb5e665ddf7fc13e02246>
  - out_i: 006, g_idx: 1136, xmr: 400.00000000

 - mixin no: 3, block height: 277893, timestamp: 2014-10-27 02:39:33, time_diff: 1 y, 173 d, 16 h, 18 m, 13 s
  - output's pubkey: <895fb31a987e4d8eed81477d4f4983cc40c54b8acf31a8cf99da540a0391851c>
  - in tx with hash: <588220d117c66bb18e6785edb81fb80f884417d95f3ec85ec4895d0d73a3c83d>
  - this tx pub key: <d6b2c26d21477753cc5c9e8ff4ab98e08336ef9925f0b814d9881d4b6ac1980e>
  - out_i: 003, g_idx: 1289, xmr: 400.00000000

 - mixin no: 4, block height: 304083, timestamp: 2014-11-14 12:37:45, time_diff: 1 y, 155 d, 6 h, 20 m, 1 s
  - output's pubkey: <ae3f33e5437878f9a1aa67f61bf16edbbeead6473b1f51efddc20cea9bc419aa>
  - in tx with hash: <b356657415bb6279462be6fae106b948fd11a17a5abddb2eb2d7d17ca4401edc>
  - this tx pub key: <4f34a7da4c56bde4542fc675bdbd11f3dcc00012c4d5db09e94dc5904565003f>
  - out_i: 005, g_idx: 1385, xmr: 400.00000000

 - mixin no: 5, block height: 323497, timestamp: 2014-11-28 01:43:53, time_diff: 1 y, 141 d, 17 h, 13 m, 53 s
  - output's pubkey: <67e77670b49a0f7a8de4edd113012788945559e12a4d48a8abcca0921b437968>
  - in tx with hash: <27b1c8a1953f45dcfedad059b7dbe129a0f937685692f943d43c6d48ab81fe22>
  - this tx pub key: <70c5ad5dfd402eeeb9835761442b05823e34030abba443bf3459fb8f440c0d9b>
  - out_i: 004, g_idx: 1468, xmr: 400.00000000

 - mixin no: 6, block height: 328177, timestamp: 2014-12-01 09:49:43, time_diff: 1 y, 138 d, 9 h, 8 m, 3 s
  - output's pubkey: <a631bb1690769626ecd5975dac11e6a3fe0fc0ed05c5ab8f8d0a84c988b028dc>
  - in tx with hash: <faa2ff63a9dd81d24e5435d0c3f6533775c53e48893cd97067c34b0f00fcf334>
  - this tx pub key: <8249ac3649fd67b08d179156b7882632eaff65d61333d39af542e5930ce2125d>
  - out_i: 010, g_idx: 1495, xmr: 400.00000000

 - mixin no: 7, block height: 408410, timestamp: 2015-01-26 15:49:54, time_diff: 1 y, 82 d, 3 h, 7 m, 52 s
  - output's pubkey: <9369c60022124a05696ea4a075d40d836d8d459a45248863feff91246c65fc72>
  - in tx with hash: <17a2bc24b762ce1c89bb50a5676e6f3035a835b436a58840588ee04d626b4a85>
  - this tx pub key: <8b8b43561f3c88057623017e6a48eea85b42990d82b0466fe0549b116bcf7163>
  - out_i: 002, g_idx: 1962, xmr: 400.00000000

 - mixin no: 8, block height: 414091, timestamp: 2015-01-30 15:44:05, time_diff: 1 y, 78 d, 3 h, 13 m, 41 s
  - output's pubkey: <8fe5459c9c9ad7f1a05ddd9c13ac4a50f0a1a2898ebaf8c4426e20265db6ee58>
  - in tx with hash: <97ede9f5685070addd0db1bd9fe2a2dd20da0de84a09a97751252c78cf22d7b6>
  - this tx pub key: <dd91950d3069cd910c0afa2ebfcc0ce31cb27ce302666a5ee090bed98397b971>
  - out_i: 002, g_idx: 2044, xmr: 400.00000000

 - mixin no: 9, block height: 432452, timestamp: 2015-02-12 11:12:43, time_diff: 1 y, 65 d, 7 h, 45 m, 3 s
  - output's pubkey: <2e38b390744e260a3044138fffb7a4cdf81f404230104ff8c0bdcca1ce226b17>
  - in tx with hash: <475aa564c58b949001ff78183030c71b90173add3bd16086ed2b604a8b9f644f>
  - this tx pub key: <27f85c0b00a77881335a75e10489047ca858502ed94c7fbddea158a637e5d7e6>
  - out_i: 005, g_idx: 2163, xmr: 400.00000000

 - mixin no: 10, block height: 469459, timestamp: 2015-03-10 06:49:20, time_diff: 1 y, 39 d, 12 h, 8 m, 26 s
  - output's pubkey: <2ba29d1fade4500895d11c030706686f9a7e381ee88b3b65b7d59f6add4b92d7>
  - in tx with hash: <fb30219d35ea16d80673e0dbf24cd2913ca954e0f5c3391b98f37e02a28a41c1>
  - this tx pub key: <15f150e19502a02e6021d35534239e9fc75a42d179d1459ca3129a5e41f8f29e>
  - out_i: 000, g_idx: 2399, xmr: 400.00000000

 - mixin no: 11, block height: 518075, timestamp: 2015-04-13 04:36:32, time_diff: 1 y, 5 d, 14 h, 21 m, 14 s
  - output's pubkey: <3abeb746ea890457d1f7f3524b3dd0d1d910d4205a75e6212c574a60f3708a0e>
  - in tx with hash: <2ee7c9ab31a241fe7997f0726d58ffd1afc9180f59ef3362ce7cc0521b19f7a8>
  - this tx pub key: <10bcbce704d6d490f9d5fe71da1ea7b176365685c33d2c4d538f73e114f4cc9c>
  - out_i: 007, g_idx: 2701, xmr: 400.00000000

 - mixin no: 12, block height: 538209, timestamp: 2015-04-27 06:50:43, time_diff: 0 y, 356 d, 12 h, 7 m, 3 s
  - output's pubkey: <3b6809f60a0e62f44ee75faa0605c6d7643d699c7431e331ec1de469137b8e19>
  - in tx with hash: <d20899d813db92a4af4a6609da1995603afeb422c6c672d26ca89519ee54cee9>
  - this tx pub key: <b2fa1d9bdfeed71e28f5256b145e820abdd3fb4d7254ddff9f149fcd2d57da3d>
  - out_i: 000, g_idx: 2825, xmr: 400.00000000

 - mixin no: 13, block height: 557812, timestamp: 2015-05-11 00:02:16, time_diff: 0 y, 342 d, 18 h, 55 m, 30 s
  - output's pubkey: <0fd97d794247a37edfa88906b80fb0cc1325520ac672720b21e9848026004f4e>
  - in tx with hash: <9abbb870a5929c5828cbe3e1725122476413741541b848fc1bb8fca0f0822544>
  - this tx pub key: <9561aa0fe2917d28609b3761dbe6f1df06bc8cfa3fb8ff4cd8343adda84b81c6>
  - out_i: 009, g_idx: 2948, xmr: 400.00000000

 - mixin no: 14, block height: 575487, timestamp: 2015-05-23 07:22:02, time_diff: 0 y, 330 d, 11 h, 35 m, 44 s
  - output's pubkey: <0b2661d64daf5b25d570161fc778f7828bfcb33aed44455787a780d8da55ac42>
  - in tx with hash: <85f8f0c7e8388869fe8a58188ebd73bf6408ed0bb30a10b55cf5873c8a785f03>
  - this tx pub key: <a05a81e8b9f0d131830acc3a54e1f20f843e97a3ac4c1d0c197893a7f951f149>
  - out_i: 005, g_idx: 3170, xmr: 400.00000000

 - mixin no: 15, block height: 604091, timestamp: 2015-06-12 09:45:28, time_diff: 0 y, 310 d, 9 h, 12 m, 18 s
  - output's pubkey: <ab63125b0e22783a9fc4cf7b67fb787b773a73a185ffa96a4b921ee51c2c7653>
  - in tx with hash: <57a6a7abdd94aa7500d15a704182f850493f7832efec5c051a49dc5d12ad065f>
  - this tx pub key: <b7734b09ce9957bfcf781ff35e311df1d012790b6d9e20565f3f03e5464b837e>
  - out_i: 003, g_idx: 3402, xmr: 400.00000000

 - mixin no: 16, block height: 644654, timestamp: 2015-07-10 18:19:59, time_diff: 0 y, 282 d, 0 h, 37 m, 47 s
  - output's pubkey: <009e7df99f6b61df53b8a01dae93abbc667d199f763b0222e6d8a542b392d9df>
  - in tx with hash: <e6f88c32e81941db52fffbb39af5264806c980b3d6629b9d9899c75a4286d706>
  - this tx pub key: <13aa354a353a832be1e0bd06ea2c30ac11ec44d6871a008dbde5a80653efc034>
  - out_i: 000, g_idx: 3687, xmr: 400.00000000

 - mixin no: 17, block height: 670873, timestamp: 2015-07-29 01:30:45, time_diff: 0 y, 263 d, 17 h, 27 m, 1 s
  - output's pubkey: <5a3d48bf9ca62a5edf50880a1f0b48eccf954d146e387b27057f8421e1a7168f>
  - in tx with hash: <6e804648e5e2ad411ee1ec8d7575976e196d22dfdcb86c2749aba5cb65ee93be>
  - this tx pub key: <b1cec6c1c621a26a9fdca65881e5de2d1036a44bfc438135067583d4743d2f6c>
  - out_i: 000, g_idx: 3877, xmr: 400.00000000

 - mixin no: 18, block height: 674014, timestamp: 2015-07-31 07:18:29, time_diff: 0 y, 261 d, 11 h, 39 m, 17 s
  - output's pubkey: <6cb0b13ebaed5365744b8a3068f9204d982794cc6ec10cbcac5a037e77b32fa7>
  - in tx with hash: <3d7d5cd0a5c3b3dec97d642a2b1833a20a587a0e67898ced939be86667892f41>
  - this tx pub key: <f13d0d7379f4d6e4b60a0cb32198af04a9875168d0edf480bd64e79cabadd0f6>
  - out_i: 006, g_idx: 3912, xmr: 400.00000000

 - mixin no: 19, block height: 692007, timestamp: 2015-08-12 20:57:36, time_diff: 0 y, 248 d, 22 h, 0 m, 10 s
  - output's pubkey: <b40bdc32d4a32f15b23bbeb701ff4fd46fec4b2d0c42ca50d4fbcb2cd9f0d190>
  - in tx with hash: <b032b97b5c73d489e309371bf086eb1de782f6608c2e99df4704d38968d7c7f1>
  - this tx pub key: <854d53cfc33b8ec184328474034993f7658ba17f1318f98f4fb11cf9e424ee8a>
  - out_i: 008, g_idx: 4128, xmr: 400.00000000

 - mixin no: 20, block height: 699633, timestamp: 2015-08-18 05:38:41, time_diff: 0 y, 243 d, 13 h, 19 m, 5 s
  - output's pubkey: <2185f0c735e086df3aa1e774c4e6f2e263203cc0118e57b7f2966a348a14b86a>
  - in tx with hash: <a31887c5b7c937a6438ef8f531d57538b19bc5d590831c45d06a26ac72b6126c>
  - this tx pub key: <ecf7df547f9dbe5eec1b0b2c3046a7c8fd5eb701061b80685f4efeba39e39182>
  - out_i: 006, g_idx: 4199, xmr: 400.00000000

 - mixin no: 21, block height: 732883, timestamp: 2015-09-10 14:26:33, time_diff: 0 y, 220 d, 4 h, 31 m, 13 s
  - output's pubkey: <e7a8f44e26664608ace09c10af209836d1f1b5c98fbd010715cdfbfb863f300b>
  - in tx with hash: <8b3ebe3a847b8fd466d412c5c2f7cf7837d482cfef10b4065f8dba6e234ee9ad>
  - this tx pub key: <094604f5b6cbef636ef5e231a034af7d6f70da8fd86b466c146e6d843d4db022>
  - out_i: 000, g_idx: 4514, xmr: 400.00000000

 - mixin no: 22, block height: 739215, timestamp: 2015-09-15 00:07:25, time_diff: 0 y, 215 d, 18 h, 50 m, 21 s
  - output's pubkey: <74fd228e7123d693a1d58f7de7baf4b377f3bdb2c1c75d39592649deaa41b047>
  - in tx with hash: <5cab6551f593db9550090fb3d72077158fd0e4904d70d6b893bd02a0c264946e>
  - this tx pub key: <f581c88e019e22fd89023565afea232cd7f34c352aa101dd5226a08bd81b43bf>
  - out_i: 005, g_idx: 4571, xmr: 400.00000000

 - mixin no: 23, block height: 739930, timestamp: 2015-09-15 13:58:25, time_diff: 0 y, 215 d, 4 h, 59 m, 21 s
  - output's pubkey: <29a823a8767573c217a1712f37fb783a769c94b713e03b24c4962fa6cf5cacd9>
  - in tx with hash: <6f80c81f04fb99964de5df218748acfca540e24d83f2b0ebbc24fbd2c889a721>
  - this tx pub key: <67f28c14bf72618b40fc1e673216a9ee2662935bf861c76b077c22c34e0454d0>
  - out_i: 007, g_idx: 4581, xmr: 400.00000000

 - mixin no: 24, block height: 752535, timestamp: 2015-09-24 09:32:36, time_diff: 0 y, 206 d, 9 h, 25 m, 10 s
  - output's pubkey: <8becbd6bc22439d551748e699b52eb85055611377cfc5766a850f995a9b455ac>
  - in tx with hash: <d2b2aa890450acc894b974a3411c046cf0bdf2437a1478fda25f93d3caa1f363>
  - this tx pub key: <2e9a43a43415a45105f449052cbd71a4eb70a0d08fa234a827a90dc3a945d61f>
  - out_i: 007, g_idx: 4698, xmr: 400.00000000

 - mixin no: 25, block height: 761970, timestamp: 2015-09-30 22:42:44, time_diff: 0 y, 199 d, 20 h, 15 m, 2 s
  - output's pubkey: <3ecb9e2010abf42801f02faa84a15353e72897cfac9192a20d0ad4b77c2d47ea>
  - in tx with hash: <1cdc3915bd03a2aa44bb8eaf46b932fdf66ae177b3d9dcb97db1f617e0aecc24>
  - this tx pub key: <5a93a6b69cb3a66495e902f1e25f386f3c66d88be31fc7221206507027485f5d>
  - out_i: 009, g_idx: 4780, xmr: 400.00000000

 - mixin no: 26, block height: 763522, timestamp: 2015-10-02 01:45:23, time_diff: 0 y, 198 d, 17 h, 12 m, 23 s
  - output's pubkey: <19a033d74544740a3458676d62eedd61bb134e2d55b73a8bce96e646ebe2ac44>
  - in tx with hash: <cf2237ad14ad11cfc497b09a98a9da419083da4093d1cd0f6e67204dea7605ea>
  - this tx pub key: <a6af75555aaad712965de5d551883948d26a5fcca6874e754eb2ef9270139f01>
  - out_i: 003, g_idx: 4795, xmr: 400.00000000

 - mixin no: 27, block height: 764167, timestamp: 2015-10-02 14:07:52, time_diff: 0 y, 198 d, 4 h, 49 m, 54 s
  - output's pubkey: <e3515d7af5df918ca98aacb02732efdf858d5c81fca786b1c2269d050338ff51>
  - in tx with hash: <4ee6112beb6097aa65631152c755207b55182059fa65814b017179b86797c0ce>
  - this tx pub key: <23323c40d46ec222d392531c92e7892a72b72ac19956c618c2194bbdac2f5e3b>
  - out_i: 005, g_idx: 4800, xmr: 400.00000000

 - mixin no: 28, block height: 769143, timestamp: 2015-10-06 00:40:06, time_diff: 0 y, 194 d, 18 h, 17 m, 40 s
  - output's pubkey: <59fa2dcd0e528a2e11c9fc8fa5c7f9ee36ef294594de269e02c4b4ae8c5cdc96>
  - in tx with hash: <b3a375b803b881b7f47cae38160f5bc2d50207cad08ad313bf5aa296a0f4410b>
  - this tx pub key: <6c5607cc918b690594d8e18a577c24995e4085e0d9cbacfc9262aa1166f06d84>
  - out_i: 009, g_idx: 4824, xmr: 400.00000000

 - mixin no: 29, block height: 789357, timestamp: 2015-10-20 05:46:23, time_diff: 0 y, 180 d, 13 h, 11 m, 23 s
  - output's pubkey: <616cfcd7791961689f4b70c290dc26e28448565a8daadc677e0ce32c280784eb>
  - in tx with hash: <075efe8a328a36ff99417666cb16694c02f2217e3374ea63b7076d2c07024942>
  - this tx pub key: <7a0d5f0436975096eb049e15bc78fe94c4a0f55ce4129eb85ac1ed275de3d19d>
  - out_i: 004, g_idx: 5029, xmr: 400.00000000

 - mixin no: 30, block height: 859992, timestamp: 2015-12-09 00:23:11, time_diff: 0 y, 130 d, 18 h, 34 m, 35 s
  - output's pubkey: <c4c117156a6a58d63d4cf8ea9ccd67d8b20c6091b19cdd4b2712f0cda09a371d>
  - in tx with hash: <a2c5858df6739bd7a9f6cf7e5429d920a21658c4f3d6d8a1416f6dda7c1ae5a2>
  - this tx pub key: <01ab1016f3bac071ecc3aa25a07093db75d6f0aba068de03f5f2e1a6e349fc19>
  - out_i: 004, g_idx: 5525, xmr: 400.00000000

 - mixin no: 31, block height: 892242, timestamp: 2015-12-31 20:11:00, time_diff: 0 y, 107 d, 22 h, 46 m, 46 s
  - output's pubkey: <62ce6f3f75e5070cbd5b3fae1652068a23a77de7cfa9fdd0411e55365422ef7c>
  - in tx with hash: <37cbc98adf68995b7210d4839bdca590ff23a988b1404cce59e3a13829963adb>
  - this tx pub key: <88b3d722a8d1778e2391c24dbfac3f3938b1a29bac78072cc3992bea18946981>
  - out_i: 008, g_idx: 5750, xmr: 400.00000000

 - mixin no: 32, block height: 981914, timestamp: 2016-03-04 07:01:16, time_diff: 0 y, 44 d, 11 h, 56 m, 30 s
  - output's pubkey: <9898f60b86340dc184dec69fe09970e21f776832f82adea3a253a6de81f06010>
  - in tx with hash: <aa59a81c83e5809c47eaa6e7a80fb44b03756e59717ecfaa71ae69efdea64487>
  - this tx pub key: <a2ec5525aa1040749614db7612fd7e58d5b41ea68e4b32cafc0389cc3b4e26a8>
  - out_i: 004, g_idx: 6627, xmr: 400.00000000

 - mixin no: 33, block height: 982649, timestamp: 2016-03-04 19:04:35, time_diff: 0 y, 43 d, 23 h, 53 m, 11 s
  - output's pubkey: <e7223fab345c35bd4fa096b9f2cde5c539d31ef2c2021b26b4cbcc31e227c9b2>
  - in tx with hash: <e3a395c46cc062f644ef17c3e585c2ad56be53261dfb7c68120828b154f56a58>
  - this tx pub key: <c1b232b6e740fff94cdfc5f2e8a480cffd8393901159549ba9c3fa933aaaf58c>
  - out_i: 003, g_idx: 6646, xmr: 400.00000000

 - mixin no: 34, block height: 986151, timestamp: 2016-03-07 07:35:51, time_diff: 0 y, 41 d, 11 h, 21 m, 55 s
  - output's pubkey: <9ad8736e9e9c067223c7a2ed0f3d769a7b83fc2ed190dfe0878624dec1c3f9e1>
  - in tx with hash: <62369b89be6ac695befc71eeb778c93be4ad79f2d833608ac184a24970124a9a>
  - this tx pub key: <f1947719935a4aec40d377a89757182cc3c60b31f395df1c34e6648fb34c9182>
  - out_i: 005, g_idx: 6733, xmr: 400.00000000

 - mixin no: 35, block height: 1001606, timestamp: 2016-03-18 03:44:51, time_diff: 0 y, 30 d, 15 h, 12 m, 55 s
  - output's pubkey: <628472be5368f4f04097608516b7d49d399aa2389530eedd5b20d071d9b1e58e>
  - in tx with hash: <e2a478f20b1f857e2b91e701d727a874276ae0b03dd4e92b3210cb817836513b>
  - this tx pub key: <892e75dba7d2acadc372b2560996bec962fd604597805e4432c9324cdf459ea2>
  - out_i: 013, g_idx: 6908, xmr: 400.00000000

 - mixin no: 36, block height: 1005009, timestamp: 2016-03-20 14:37:48, time_diff: 0 y, 28 d, 4 h, 19 m, 58 s
  - output's pubkey: <e9a2da8d163b5022b52e56fab8164be7943aa38740ad5ef6d37a7a04e7fc1e76>
  - in tx with hash: <6cc56779178324f1b9d1bfbd9a7e0730b6273960ee5138e6dcc67636c3b05b3e>
  - this tx pub key: <4cafb4aae8387f9727286c048d2091935852ae9af59ee47a52e654c08cd8ec26>
  - out_i: 008, g_idx: 6936, xmr: 400.00000000

 - mixin no: 37, block height: 1010588, timestamp: 2016-03-25 01:54:28, time_diff: 0 y, 23 d, 17 h, 3 m, 18 s
  - output's pubkey: <b2f8a50267678598653fe81cd1099a205b3802f96abd178954168b8e7a64c969>
  - in tx with hash: <3420292a5ce15f20495ec656bdce88beb9e08b382a8acf14aa9b8407ac7beb27>
  - this tx pub key: <95d7306985db09b77db170d9ed0066818088b273c21923b77bcb37298ec718b5>
  - out_i: 016, g_idx: 7044, xmr: 400.00000000

 - mixin no: 38, block height: 1012344, timestamp: 2016-03-27 10:05:15, time_diff: 0 y, 21 d, 8 h, 52 m, 31 s
  - output's pubkey: <d31c57b454d8cf6e5ff25fa76d694f6e23760f946ef6b32844524508f66c80f9>
  - in tx with hash: <d690b4ba965bb9ea1c147c2b2850fb13baa61e3eb6a04acc5793f52252b9f069>
  - this tx pub key: <5af7caeb614030d5010f8848edf2a21cb20e3e7d153276894838dd5e789b8635>
  - out_i: 014, g_idx: 7100, xmr: 400.00000000

 - mixin no: 39, block height: 1012495, timestamp: 2016-03-27 15:17:18, time_diff: 0 y, 21 d, 3 h, 40 m, 28 s
  - output's pubkey: <604d21978953eab3a17c4f81c9676ed60cea65f6f10ef4f13c53d24e639a34e5>
  - in tx with hash: <1efc90d5fcac18b2fff070a13ea5e74dce31cfa2eb4c02177f78b5c5c4eed3da>
  - this tx pub key: <e115eb882e4eefa531953b9d2bc67992a16ad41a5b05f1efabfb33240528cccf>
  - out_i: 006, g_idx: 7103, xmr: 400.00000000

 - mixin no: 40, block height: 1014163, timestamp: 2016-03-29 22:30:31, time_diff: 0 y, 18 d, 20 h, 27 m, 15 s
  - output's pubkey: <541ec0b9b463c42428be38f49d660b1ba029bdd332eebcd2c754dd82a6d7f43e>
  - in tx with hash: <7d2c9ec1385cad779a53be23468877647267c99dc40633e0be556799340893b1>
  - this tx pub key: <4d3d0d2c9995eda5f018a63ae1161d697ec0ea830b53315baa46183a06102676>
  - out_i: 012, g_idx: 7156, xmr: 400.00000000

 - mixin no: 41, block height: 1026806, timestamp: 2016-04-16 17:57:17, time_diff: 0 y, 1 d, 1 h, 0 m, 29 s
  - output's pubkey: <1a07fc2bad9ee201a6355226f1ae4dc343546bb4a5de3cf9516641170f683e6a>
  - in tx with hash: <4f75ebb511f9f6038049a05dd6f328a7b1a7f6a728d8c569b446ae887834ef28>
  - this tx pub key: <8f2973766ca42c9e42a3f897da2304e8576d250e03087dd170259dd8b8afe53b>
  - out_i: 004, g_idx: 7410, xmr: 400.00000000

Ring signature for the above input, i.e.,: key image <035f873ec6bf28ca4456adf72a7e60663b775b52aee580cfb33862cc1a73d24c>, xmr: 400.00000000:

 - c: <69b4314c9f96d0bc2758031c7d4db2cf5ae18190457c971c73a372a58eb3c10f> r: <72cfe5295324bcef5ee34014087e23600ffeea20ce18673a174550973191c30a>
 - c: <243a49a1e1abb82fc50859db50c9c2f6907fb54bbe7ed941ff9d9138feb90c00> r: <9cbf6a235eca235d14017c94fc25d0fa20b7c9221ea7fe06bd1871ad918a6d00>
 - c: <8d8963e185be6563d9e13a3d79d38a76dfbd5cc17141d975769801e990b95b0b> r: <e0507329a5f9cf28e3aefc4f1c06ae0ab699b5cbd52249c0b3513b15bb9c020f>
 - c: <3249eb058ffafb3bc9062f21e7586a4a9bad5df5d77aea5f897d99c5513faa05> r: <cf1587f4163e295fa834af53bc2f2047f985f1228ef092bb8bbdf1e977d4350d>
 - c: <bcf6ffeea3c08450da66930903d9463ee6e19e65b4f197ac1087c7a0a1f19409> r: <032e578bee72c50ee0152ab25b1770ca15c76d5bc69f874042d4edd76a2acb04>
 - c: <9a5731193d739a135ba39065563752bc78ef3bfa9c17800b1f841e0552f0de05> r: <59c85604120d49d9048650b099932e81cabe1f70525ac8923dc2c4439c26f00d>
 - c: <e53bed6d39ec7ad9956c58b4c9dc2ca972f5cc63f0796f1ae8b6c0bd4509c609> r: <efbe23d97d46acfa9f202e0279d2e082fe31a80e8b05ff5f6a815f0f87bf920e>
 - c: <9ba0105140710a7d2c8234f65d98cf86bbe260c44497dbb810ef0b55d6c8b90b> r: <dcc6d171461577d94584bef71d21963b932fc8dfb5324ec41300c373a79fad0c>
 - c: <9351bff3e0b0eb69d5ef9e45409427f94d6a8c24d0d522b772e21e9b8d735106> r: <8485951dc67aa37977d7db05083a3e707dbb513f3d5750f6070d6de1ae786b07>
 - c: <75a5612eb8fd9b800ce9dae6b1c897c2d76c3f76d6f445e6d6988ea044862e0f> r: <40b98083844359201c5732166bba2837228dcbe2d0e15cedb094022dfe175c0b>
 - c: <c1b060ecc1670802e35cb3a4bf118e5e0ddb4cd79ab3f105601e1538cbecc90b> r: <e66fb1c15e9acd3ef9d3ed9d358c4d7bb468c9e45a46d9e0dae13c661a7d470e>
 - c: <90adefcebde58c79195b3bd1ec84402359d8a2a7afc3e065df6ffcfd3623f801> r: <0d9a84a169bf534dbc8b0f810a1376911a8478baaf13dde63a3eefec425c0b07>
 - c: <25b9d2973e8782d368e1cad16b42865d6d9ef8990806cf279e687252a2c6ba0c> r: <f31a5f5d76ccfe52c1475e1aa05af22c23767f0fa82024ac676d6db478956403>
 - c: <d98ac91741efd586a1fdaa392a5d3125ff4b2d4a88f0f86fcb21a176211c9301> r: <e479703f44342664bbf2fef32f8c03fd05d5b33cc7eef3b33289a130a9ebe102>
 - c: <ea9eeebd085efe1e41ace8c2ed0198aa75365406aedef3e7eb6add1215cfd009> r: <04c6a8e5ef7538a43661dda0cfdcee7ec0d37dc49f0cff16651b1d36df737508>
 - c: <3ed490b8183097812c17193098cef80686f7256df125fc00ae69d7782b650902> r: <054c136bc5c014219f177079cf29b1b6180754013cd015909677c9675456ae08>
 - c: <5615bc8213364578bfc212ab215e231acc2a89bcd4f619d3f649de02175df005> r: <38807ff017ccbd592bfda884cc6f0ee526bc6b5c7166704545074ce9ee323702>
 - c: <c14a0e22b61d94bc876f92215029d6b2f7b15c8daabbde7e6f7811d6a6af1d0f> r: <062943eb47a3ea83deb72bd081c6172afcc7f43ccb855999c501f70d17944600>
 - c: <404fb20b22b3315aa4e6d5507d7058f3d76d3e27aeb6e56f9c57972d91bc2e07> r: <e8c64591ec67a238ef0daab98c8e0f9cdac33c362507a46054b00a85c9fac70d>
 - c: <9566820c7f3d9e09902dc2962142da421d4c0a59a6c14dfe41c2c515c2b71800> r: <5eaaee98769fb64afff725a5f2d2fbd8bef594c98b48736d6d69082ad8533408>
 - c: <03f41ecc6f65f26bf150bea7c6008544cc0c7da2b6f47aec51d9e7c68137fb06> r: <5e744bac7258d52b05f3e720788a2909c7c402df4a334532169c341d34bb8708>
 - c: <ccb2443d412ffc034f5f191df56d5427939a4052bb7dc6aec277fbfa05b88a00> r: <c1d50a32c9b412e7a1170f433c5ce4dc5750379d4df12e05517b645329ee1a0e>
 - c: <5e60a5dd476fef564896803d62e2553d1a5c0ed7051473c5b981bd21f3e07f00> r: <9cf13c44bb79f1d3c120236b8de37f3638772059e7f0a827729185a59eff2b05>
 - c: <a59f16fa78fb60e277ee3f72bb1360b4028c26e2b8a4d961644887461211840a> r: <f7b7f954e13301adcd36accbe25fafef4813f88bbc692a45d9ef30361b62a60f>
 - c: <414b2f0fcfbf7ffc0488c5065c4972ce268165c0664c6bed4634af4db1d46403> r: <b9e88376840d31b9cc9db7b03e2ed9c9f05adf50a37e12cc223baa4091b55706>
 - c: <431e14e38b472ef8c36e75c735b8899dc974982d22261cd9c7f341c129b10205> r: <1dbf96262d74d4178177f59ff6e034ca048bb66dcf23ac3a6aef1b34e2078205>
 - c: <fe2af9bba2c39493235e411717203783cfacf1790ce131f545628c4af27eb308> r: <7ee0e6af0c779a7fd5a30c86019074f5a143b36d7223ff903ee5d5236eba960f>
 - c: <cc2b10f72e066406e5f53481db04fda194af7bc031adb0ef0027e62f8c638e0f> r: <a92381b88652ca74345ef60b67fbe2b66ea776ce8c25c39f9a960307ccfd0102>
 - c: <375eb9f28544de67a6272f6c3be5c5b7e7fe534c181ff0072a5cadf77d4fe203> r: <6ea521e1983341e4e5bc3618ae43beef950bb1170c8b6858e04a6e9edf16d80b>
 - c: <2088f14284aaa151493b42b3d2d3a53ecbd055fbf29a4552dc425d03b47ba50a> r: <09632e3a187cc576572144415b953c3c39f353becfce6a0bea011d328cefc30a>
 - c: <38f059fe0992fc74bc0f0a0156265e5acdec3405861dbb041d020c1defd6820d> r: <4937c63f40a5a8d37ce66d8497ae1f9f91ee21ef0c188eaafbbf938d350e0709>
 - c: <16683d0138ccfe4d1773324f3333bf2caa454cc2fe18809bd56135f316c05f01> r: <3745df987e579ea3e4cd392b6050575affffeed137d5562d24ef7d5007ba840e>
 - c: <982874dcf03b79b5bdf41f8a34c16b5645d7dd7d687ec3b03448133e384d8407> r: <5815f782c358bff8365c78671095a5360eef456c23eef7b690fd310890ee470d>
 - c: <7f4eb0093ff3f81526d50a3d51b9f0a8c574405ebc514f856390fd0144911108> r: <0c28b5c5601e8cf13ae521f675d3c5a344172c7643fa8e8e307c62ff393bef01>
 - c: <13b56df02ad07f8b8a2a40e0b61c370df385a73115ebd2efc08b6e669e3a1306> r: <32b2a9ca0d5a61e1a3f5ee046c84078aaab40281da72815f89e8ecc1de404303>
 - c: <bedf06e867e2eda3bca57aa9e6bd1beb511b733650e55eaf3e1c6426d1960407> r: <a70fe08e639d4463fb3610883c31fe4809a778c2d803d735ceb0840de99f910d>
 - c: <3f23678027cfba51c4e9c526126942e517a5e5d451abd8e1cd38e4b68e57c002> r: <45e9c864a01b1277f43fd9a46ead1ccaa67c1ea0b42075ddf00f7153565d770e>
 - c: <8661a2af0365a3b3ff344fffd5b5e5203c9fb7f9dd7510158d88e19dc582a60b> r: <8c0d1646faa13a5eb63ba7626b8d8d361ef3cbe7ef5962b7a0892d7efa6f090e>
 - c: <80396d915a8855cb7d21c00b8cf29504639b555ba366118d2b459a848c3f6606> r: <b39634004ddbdabe720948ac90d2656449a464ac59a4be6497efe76156addf0a>
 - c: <6ecf948a909dcc76a6b09b1b26aa1cde097b8702d30938c71379807d18dff00d> r: <388d9e79bb7b9d5003b89b681efa975dded97b6a7617c37cf8a91480bbab2402>
 - c: <c0f33110980b3f50704a6773e7e178d8666fc48be4d14498077590bd094d770e> r: <48c818a0c198ce69a4466c1d57a452ed4b8d99a67c17f40ef7c15c9d1487bb00>


Mixin timescales for this transaction:

Genesis <___________*__***___*_*______**__*_*___**_**_*__*_*__*__*_*_____*__**_*____**________**______________*_______*______*__*_> 2016-04-17
Genesis <_________*__*______*********_*____**________*__________*______***________*____*_*_**____*__*____________**__*___*______*_> 2016-04-17
Genesis <______________*__*__*_*_**_*__*___*________*___*___*_***_____***______*_*__*____*_*____**_*_*__*_*__**__*_*____________*_> 2016-04-17
Genesis <______________________________*____*___*_____________*____*_________*_______*__*_*__***_*_*_*___*_**_*_*__*****_***_*__*_> 2016-04-17
Genesis <____________*_____________*____*__*_**________**_*___*_____*_*_*_*__*____*__**_*___**_**__*_______*___*_________*****__*_> 2016-04-17
```

## Example input and output 6
Transaction hash given along with the corresponding address and view key.
[MyMonero.com](https://mymonero.com) was used this time.

```bash
./showmixins -a 45aVogkbq3fSVvY1p8BWzA9r1HjBi26u57DrJ6iNrG7AG2r6zqpyzFqikuT863a8A8PMn2Akb21yrdw1VcYL4qJc888Np7H -v 1cdd587034c25fc3de4d09be5d856464a2484251503aa009779393a3137e5d06  -t 9040535c2b7527ca7511a9b482a5c0d3fa25a1a135a7b791bc2bd6e6ec004384
```

```bash
Top block height      : 1027566
Top block block time  : 2016-04-17 18:52:42

Payment id: not present

tx hash          : <9040535c2b7527ca7511a9b482a5c0d3fa25a1a135a7b791bc2bd6e6ec004384>, block height 920269

private view key : <1cdd587034c25fc3de4d09be5d856464a2484251503aa009779393a3137e5d06>
address          : <45aVogkbq3fSVvY1p8BWzA9r1HjBi26u57DrJ6iNrG7AG2r6zqpyzFqikuT863a8A8PMn2Akb21yrdw1VcYL4qJc888Np7H>


Input's key image: <fcd88a1a842d43cc6e787a1cb014473270226d71b37b3c631521224dfc5bc1b3>, xmr: 1.00000000

 - mixin no: 1, block height: 123158, timestamp: 2014-07-11 06:31:03, time_diff: 1 y, 281 d, 12 h, 21 m, 39 s, ours: false
  - output's pubkey: <d6b91dc24c4ef4225f83a445f4b2859fc20556c1cd6ee102fef83d1a4a9f73de>
  - in tx with hash: <8ab059b21793dd20a5381bcd4acde8b20622126e7a7d43acdaf9edf8b40c744d>
  - this tx pub key: <62e9199dca865493a69580f3c3f911396d9e04160d7d4451ab47393faf54f90e>
  - out_i: 047, g_idx: 91604, xmr: 1.00000000

 - mixin no: 2, block height: 124518, timestamp: 2014-07-12 05:05:04, time_diff: 1 y, 280 d, 13 h, 47 m, 38 s, ours: false
  - output's pubkey: <72aa5d0082f607802807976654d2bf9cec5a394b98bfef216ee64cef7a609d7b>
  - in tx with hash: <a0632b39a6bf5d2856e7746f951b87d8aa657af4cb7cf55eeb59baec0de9d211>
  - this tx pub key: <432122863e489536f32e7bb275faf953543e7b0b688b99a4b4c6c7038b7de2d4>
  - out_i: 006, g_idx: 94285, xmr: 1.00000000

 - mixin no: 3, block height: 125461, timestamp: 2014-07-12 21:04:32, time_diff: 1 y, 279 d, 21 h, 48 m, 10 s, ours: false
  - output's pubkey: <016d0fa602ed2cb2fe6a931480e260e327ec4f8f97c2bb5e742fd4b22d834372>
  - in tx with hash: <0b1163df3bfc8b8419a39c4768568eb7a38317be49a85cfa9878259a2c61811c>
  - this tx pub key: <83e64279f9ee73fec4debb792d284d13cbddbf9647a1448cb8fdd2a27aeadc18>
  - out_i: 024, g_idx: 96223, xmr: 1.00000000

 - mixin no: 4, block height: 222646, timestamp: 2014-09-18 12:10:51, time_diff: 1 y, 212 d, 6 h, 41 m, 51 s, ours: false
  - output's pubkey: <d6ea13f7090efb3131b0345439ec71bef8c0904580346a12da39cdf40eca45da>
  - in tx with hash: <cc2e9ffa7b08406f47b20bcf69d42e26cb4eeb6a3580acf82c4aa9588a4b720d>
  - this tx pub key: <88dd561a11b6c94b7f0cb06924d80af3b5d77ebcba1dc35cd843a3ec8c11733f>
  - out_i: 102, g_idx: 265927, xmr: 1.00000000

 - mixin no: 5, block height: 223459, timestamp: 2014-09-19 01:01:37, time_diff: 1 y, 211 d, 17 h, 51 m, 5 s, ours: false
  - output's pubkey: <2ad82033544840fff6c37821d8dc8d2089f0a18e332844639fd406176b2a86e3>
  - in tx with hash: <fe9affd1417dfc859f8ba49e9f195d69f9f23cb88317fe9c211dfb4551dcdb70>
  - this tx pub key: <dc4d892df7768582641e8082089318ee61d4950d855489a292ce47882f1a8a2e>
  - out_i: 054, g_idx: 266896, xmr: 1.00000000

 - mixin no: 6, block height: 306611, timestamp: 2014-11-16 06:30:13, time_diff: 1 y, 153 d, 12 h, 22 m, 29 s, ours: false
  - output's pubkey: <aef21e03281e8e5c995276a0c45a1b23c5073942c3c6e943d6b31857300a4d8b>
  - in tx with hash: <8f17dea8c98d2430b9a0ae702920e18bc5f21a8763a893cad9d45f76d70411c5>
  - this tx pub key: <1e8ef1b9ee784008fa4897c03dc0c56fa3b94605d6245f5266b55bcbb85f64fa>
  - out_i: 056, g_idx: 337266, xmr: 1.00000000

 - mixin no: 7, block height: 331494, timestamp: 2014-12-03 17:01:13, time_diff: 1 y, 136 d, 1 h, 51 m, 29 s, ours: false
  - output's pubkey: <a25fe80d01f78a2e551e1974a3ea9789daac0c812af6d94e20211367b3df4702>
  - in tx with hash: <e70285f78c5213bc42f7206204c2dfadeda6ae87ab80d4635598b6a285838514>
  - this tx pub key: <3c287534e5248969777c7f55eca5e2e8a265a5092dee8b97118b35a87a445674>
  - out_i: 050, g_idx: 355827, xmr: 1.00000000

 - mixin no: 8, block height: 405972, timestamp: 2015-01-24 22:18:20, time_diff: 1 y, 83 d, 20 h, 34 m, 22 s, ours: false
  - output's pubkey: <5edb09ea34f4eca7d32bf2e5a2d86bd5816784cdc3ce99be3ae1b131f8c63cad>
  - in tx with hash: <79a22fea87b26ba1e48ae0692784c99485d204352babd82a0be3a7257457d19d>
  - this tx pub key: <9c51c755cdeb00297167ae5565ff0cda8bf9f17bdc2388d6fd970cefaac726b2>
  - out_i: 003, g_idx: 394726, xmr: 1.00000000

 - mixin no: 9, block height: 439421, timestamp: 2015-02-17 08:44:37, time_diff: 1 y, 60 d, 10 h, 8 m, 5 s, ours: false
  - output's pubkey: <bd4ed61db66405f48c418fbae3db4f6d51c230a3e81ccee378b4defef7f65490>
  - in tx with hash: <00d3ba5a8b68f93182603ba0459307e42861835b14ab2e740b610dcfbdb8a10e>
  - this tx pub key: <a097c65ee7e09d1942e55e6743d3cb3586130ce67dbe2b4a70441d38e03e4287>
  - out_i: 003, g_idx: 441108, xmr: 1.00000000

 - mixin no: 10, block height: 488609, timestamp: 2015-03-23 15:55:21, time_diff: 1 y, 26 d, 2 h, 57 m, 21 s, ours: false
  - output's pubkey: <3535cef80eae8378602cdc1023810b72a6f732d4a92ca22fba7198eb72563e60>
  - in tx with hash: <df22d03e144bcf6dac519fcaf4ebdc9d302ceb969fba5fffe5af5049bf6f18df>
  - this tx pub key: <217ecee91c0b2b44a57dfb3001f53cbe7b873d72c8f388ea6266a63df63200a8>
  - out_i: 002, g_idx: 512487, xmr: 1.00000000

 - mixin no: 11, block height: 920251, timestamp: 2016-01-20 17:32:47, time_diff: 0 y, 88 d, 1 h, 19 m, 55 s, ours: true
  - output's pubkey: <0247cb9d7a2886632f4ae57295e6ad1f3eb0455cb2735896f18c4f11cd57e9e7>
  - in tx with hash: <d503cf26b56d1797f814fffd165da735bfbd94961c1e0399fe46600e67e9c739>
  - this tx pub key: <5654bb04d0ed5fd7d8d04292aa5b5ddc84263fcfdccfa39942c6f87266fd1126>
  - out_i: 002, g_idx: 673104, xmr: 1.00000000

Ring signature for the above input, i.e.,: key image <fcd88a1a842d43cc6e787a1cb014473270226d71b37b3c631521224dfc5bc1b3>, xmr: 1.00000000:

 - c: <84f751550811dcfbb8975fac17668ecefff6f6a6c3ce1bb905c7505820726803> r: <f0c7c23146237ab9cfc57ac8973c7c3c46e1c7e8e6f8a117a14721735d273705>
 - c: <8838c9ecee9718c323722ae4b15ba5023914819fea0f857904470e49cde9030d> r: <fef98dbcc6b55171e79bbc8359a064d63a068927bf81bd3fcf9be8bddda00e00>
 - c: <f29c0ec928be5d0749633967f7164fe2807a24b5bf660bc231302ce1a9563a02> r: <0ff5dda9fc459ab476de7978185191ff98ec62f9fa4c4c9377764381b4465302>
 - c: <740857d919576413d8581aab4d654ecd9f1625fa3485a6fcebda83cb81fa4e0c> r: <6c5e9b88e503047ebecc063c42dfefcfb3df50a2c31a47bba4e95ddb4ffe080e>
 - c: <6f2921ef03b56ca56cf6f6fbd7b02518a3181e60352296edaddb5b41b6778405> r: <4ae3ef9b109772c6a0751d1c8af9a0e5e701f2ebc5208601414b628f2f4c7d02>
 - c: <14c6b657fae59f684d9431c430af211c1b0b2169cf44378f2c6a8a03c4c1db0e> r: <35eee987e5ed302b3aca5bd8a0678936227171dbfcc83210475b1a9cefd87301>
 - c: <e33f356319ed992e4d8d7ebc941163ffd0cb1dd92d3147b4fae69d69de990b00> r: <cf7eb23d3d3c623cf44a6a5afaebc81c23d6a3158dcabda9d363bf81b9fc6101>
 - c: <6a05f4fc38dd996068a9fffdc1dbdfe8005d5988656c9b8e9f1e5647b1b97d00> r: <a8142a090410f31e956674219c1565850e0bb4a98f265a9c57f378dd506e9306>
 - c: <e95f75e3d001ddd4e1feda5fc2512b4645e651e4cd3422201bee37cbccd8c50f> r: <4bf529da422548cf6c0629af9efce5ed90658c2243b8708fc71b290f59078d03>
 - c: <809bf3961b3e99012461460acf90a852adb2d9eb247e78a009ce0dce20cfcb00> r: <0e7ce7a8c9f94314d295f306e372c0a158918de9eca1393eae799b4501527c06>
 - c: <6c1757e7be71c1ce82edd4e0d1e50d5c387e247eff21770e900362c5a923030d> r: <7a62835ad4080b8023b2260d9703d5fbcdc1c3e4cf0547e80240bdc76e5ab10d>


Mixin timescales for this transaction:

Genesis <_____________*___________*________*__*________*___*____*_________________________________________________*_______________> 2016-04-17
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

After this, `showmixins` executable file should be present in show-mixins-for-tx
folder. How to use it, can be seen in the above example outputs.


## How can you help?

Constructive criticism, code and website edits are always good. They can be made through github.

Some Monero are also welcome:
```
48daf1rG3hE1Txapcsxh6WXNe9MLNKtu7W7tKTivtSoVLHErYzvdcpea2nSTgGkz66RFP4GKVAsTV14v6G3oddBTHfxP6tU
```
