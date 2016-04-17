#include "src/MicroCore.h"
#include "src/CmdLineOptions.h"

#include "ext/format.h"

using namespace std;
using namespace fmt;

using xmreg::operator<<;
using boost::filesystem::path;

namespace epee {
    unsigned int g_test_dbg_lock_sleep = 0;
}

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
    bool testnet     = *(opts.get_option<bool>("testnet"));


    // get the program command line options, or
    // some default values for quick check
    string tx_hash_str = tx_hash_opt ?
                         *tx_hash_opt :
                         "09d9e8eccf82b3d6811ed7005102caf1b605f325cf60ed372abeb4a67d956fff";


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
        if (!xmreg::parse_str_address(*address_opt,  address, testnet))
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

    // get tx payment id if present
    // checks for encrypted id first, and then for normal

    crypto::hash8 encrypted_payment_id;

    if (xmreg::get_encrypted_payment_id(tx, encrypted_payment_id))
    {
        print("\nPayment id (encrypted): {:s}\n", encrypted_payment_id);

    }
    else
    {
        crypto::hash payment_id;

        if (xmreg::get_payment_id(tx, payment_id))
        {
            print("\nPayment id: {:s}\n", payment_id);
        }
        else
        {
            print("\nPayment id: not present\n");
        }
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

    time_t server_timestamp {std::time(nullptr)};

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


        print("Input's key image: {}, xmr: {:0.8f}\n",
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

        vector<uint64_t> mixin_timestamps;

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

            // save mixin timestamp for later
            mixin_timestamps.push_back(blk_timestamp);

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

            print("  - out_i: {:03d}, g_idx: {:d}, xmr: {:0.8f}\n",
                  output_index, global_out_idx, xmreg::get_xmr(found_output.amount));

            ++count;
        } // for (const uint64_t& i: absolute_offsets)


        // get mixins in time scale for visual representation
        string mixin_times_scale = xmreg::timestamps_time_scale(mixin_timestamps,
                                                                server_timestamp);

        print("\nMixins timescale and ring signature for the above input, i.e.,: key image {}, xmr: {:0.8f}: \n",
              tx_in_to_key.k_image, xmreg::get_xmr(tx_in_to_key.amount));

        cout << "Genesis <" << mixin_times_scale
             << ">" <<  xmreg::timestamp_to_str(server_timestamp, "%F")
             << endl;

        for (const crypto::signature &sig: tx.signatures[in_i])
        {
            cout << " - " << xmreg::print_sig(sig) << endl;
        }

        cout << endl;

    } // for (size_t in_i = 0; in_i < input_no; ++in_i)

    cout << "\nEnd of program." << endl;

    return 0;
}
