#include "src/MicroCore.h"
#include "src/CmdLineOptions.h"
#include "src/tools.h"

using namespace std;

using xmreg::operator<<;
using boost::filesystem::path;

unsigned int epee::g_test_dbg_lock_sleep = 0;


int main(int ac, const char* av[]) {

    // get command line options
    xmreg::CmdLineOptions opts {ac, av};

    auto help_opt = opts.get_option<bool>("help");

    // if help was chosen, display help text and finish
    if (*help_opt)
    {
        return 0;
    }

    // get other options
    auto tx_hash_opt = opts.get_option<string>("txhash");
    auto viewkey_opt = opts.get_option<string>("viewkey");
    auto address_opt = opts.get_option<string>("address");
    auto bc_path_opt = opts.get_option<string>("bc-path");



    // get the program command line options, or
    // some default values for quick check
    string tx_hash_str = tx_hash_opt ? *tx_hash_opt : "66040ad29f0d780b4d47641a67f410c28cce575b5324c43b784bb376f4e30577";
    string viewkey_str   = viewkey_opt ? *viewkey_opt : "1ddabaa51cea5f6d9068728dc08c7ffaefe39a7a4b5f39fa8a976ecbe2cb520a";
    string address_str = address_opt ? *address_opt : "48daf1rG3hE1Txapcsxh6WXNe9MLNKtu7W7tKTivtSoVLHErYzvdcpea2nSTgGkz66RFP4GKVAsTV14v6G3oddBTHfxP6tU";


    path blockchain_path;

    if (!xmreg::get_blockchain_path(bc_path_opt, blockchain_path))
    {
        // if problem obtaining blockchain path, finish.
        return 1;
    }

    cout << "Blockchain path: " << blockchain_path << endl;

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


    // parse string representing given private viewkey
    crypto::secret_key prv_view_key;

    if (!xmreg::parse_str_secret_key(viewkey_str, prv_view_key))
    {
        cerr << "Cant parse view key: " << viewkey_str << endl;
        return 1;
    }

    crypto::hash tx_hash;

    if (!xmreg::parse_str_secret_key(tx_hash_str, tx_hash))
    {
        cerr << "Cant parse tx hash: " << tx_hash_str << endl;
        return 1;
    }

    // parse string representing given monero address
    cryptonote::account_public_address address;

    if (!xmreg::parse_str_address(address_str,  address))
    {
        cerr << "Cant parse string address: " << address_str << endl;
        return 1;
    }


    // lets check our keys
//    cout << "\n"
//         << "tx_hash          : " << tx_hash << "\n"
//         << "private view key : " << prv_view_key << "\n"
//         << "address          : " << address << "\n"
//         << endl;

    cout << "\n"
         << "tx_hash          : " << tx_hash << "\n"
         << endl;

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


        cout << "Input's Key image: " << tx_in_to_key.k_image
             << ", amount: " << cryptonote::print_money(tx_in_to_key.amount) << endl;


        // get absolute offsets of mixins
        std::vector<uint64_t> absolute_offsets
                = cryptonote::relative_output_offsets_to_absolute(tx_in_to_key.key_offsets);

        std::vector<cryptonote::output_data_t> outputs;
        core_storage.get_db().get_output_key(tx_in_to_key.amount, absolute_offsets, outputs);

        size_t count = 0;

        for (const uint64_t& i: absolute_offsets)
        {
            cryptonote::output_data_t output_index;

            // get tx hash and output index for output
            if (count < outputs.size())
            {
                output_index = outputs.at(count);
            }
            else
            {
                output_index = core_storage.get_db().get_output_key(tx_in_to_key.amount, i);
            }

            cout << " - mixin no: " << count + 1 << ", block height: " << output_index.height << "\n"
                 << "   - output's pubkey: " << output_index.pubkey << endl;


            crypto::hash tx_hash;
            cryptonote::transaction tx_found;

            // find tx_hash with given output
            if (!mcore.get_tx_hash_from_output_pubkey(
                    output_index.pubkey,
                    output_index.height,
                    tx_hash, tx_found))
            {
                cout << " - cant find tx_hash for ouput: " <<   output_index.pubkey << endl;
                continue;
            }

            cout << "   - in tx with hash: " << tx_hash;

            cryptonote::tx_out found_output;

            if (!mcore.find_output_in_tx(tx_found, output_index.pubkey, found_output))
            {
                cout << " - cant find tx_out for ouput: " <<   output_index.pubkey << endl;
                continue;
            }

            cout << ", " << cryptonote::print_money(found_output.amount)
                 << endl;

            ++count;
        }

        cout << endl;
    }



    cout << "\nEnd of program." << endl;

    return 0;
}
