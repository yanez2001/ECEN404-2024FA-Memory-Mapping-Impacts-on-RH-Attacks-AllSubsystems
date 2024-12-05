#include <vector>
#include <bitset>
#include <fstream>

#include "base/base.h"
#include "dram/dram.h"
#include "addr_mapper/addr_mapper.h"
#include "memory_system/memory_system.h"
class MINE final : public IAddrMapper, public Implementation {
  RAMULATOR_REGISTER_IMPLEMENTATION(IAddrMapper, MINE, "MINE", "Applies a My method mapping to the address.");

public:
  IDRAM* m_dram = nullptr;
  int m_num_levels = -1;
  std::vector<int> m_addr_bits;
  Addr_t m_tx_offset = -1;
  int m_col_bits_idx = -1;
  int m_row_bits_idx = -1;

  static std::vector<std::string> previous;
  static int total_differing_bits;
  static int total_bits_compared;

  void init() override { };
 
  void setup(IFrontEnd* frontend, IMemorySystem* memory_system) {
    m_dram = memory_system->get_ifce<IDRAM>();
    const auto& count = m_dram->m_organization.count;
    m_num_levels = count.size();
    m_addr_bits.resize(m_num_levels);

    for (size_t level = 0; level < m_addr_bits.size(); level++) {
      m_addr_bits[level] = calc_log2(count[level]);
    }
    m_addr_bits[m_num_levels - 1] -= calc_log2(m_dram->m_internal_prefetch_size);

    int tx_bytes = m_dram->m_internal_prefetch_size * m_dram->m_channel_width / 8;
    m_tx_offset = calc_log2(tx_bytes);

    try {
      m_row_bits_idx = m_dram->m_levels("row");
    } catch (const std::out_of_range& r) {
      throw std::runtime_error(fmt::format("Organization \"row\" not found in the spec, cannot use linear mapping!"));
    }

    m_col_bits_idx = m_num_levels - 1;
  }

  void apply(Request& req) override {
    req.addr_vec.resize(m_num_levels, -1);
    Addr_t addr = req.addr >> m_tx_offset;

    req.addr_vec[m_col_bits_idx] = slice_lower_bits(addr, 2);
    for (int lvl = 0; lvl < m_row_bits_idx; lvl++) {
      req.addr_vec[lvl] = slice_lower_bits(addr, m_addr_bits[lvl]);
    }

    req.addr_vec[m_row_bits_idx] = addr;

    // Caesar cipher shift------------------------------------------------------------------------------------------------
    int caesar_shift_1 = 15;
    if (m_row_bits_idx >= 0 && m_row_bits_idx < m_num_levels) {
      int total_row_space = 1 << m_addr_bits[m_row_bits_idx];
      req.addr_vec[m_row_bits_idx] = (req.addr_vec[m_row_bits_idx] + caesar_shift_1) % total_row_space;
    }

    // Store previous binary representation---------------------------------------------------------------------------------
    std::vector<int> bit_limits = {0, 0, 2, 2, 15, 10};
    std::vector<std::string> og;

    for (size_t i = 0; i < req.addr_vec.size(); ++i) {
      int value = req.addr_vec[i];
      int bit_limit = bit_limits[i];
      if (bit_limit == 0) {
        og.push_back("--");
      } else {
        int limited_value = value & ((1 << bit_limit) - 1);
        og.push_back(std::bitset<32>(limited_value).to_string().substr(32 - bit_limit));
      }
    }

    // Compare to previous and calculate power consumption---------------------------------------------------------------------
    if (!previous.empty() && previous.size() == og.size()) {
      for (size_t i = 0; i < og.size(); ++i) {
        if (previous[i] != "--" && og[i] != "--") {
          std::bitset<32> prev_bits(previous[i]);
          std::bitset<32> og_bits(og[i]);
          int differing_bits = (prev_bits ^ og_bits).count();
          total_differing_bits += differing_bits;
          total_bits_compared += bit_limits[i];
        }
      }
     
      //std::cout << "Start of new address: \n";
      //std::cout << "Total cumulative differing bits across all calls: " << total_differing_bits << "\n";
      //std::cout << "Total cumulative bits compared across all calls: " << total_bits_compared << "\n";
     
      double power_com = static_cast<double>(total_differing_bits) / total_bits_compared;
      std::cout << "Total power consumption: " << power_com * 100 << "%" << "\n";
    }
    /*
    std::cout << "Binary representation of previous with bit limit:" << std::endl;
    for (const auto& binary_value : previous) {
      std::cout << binary_value << "\n";
    }
    */
   
    previous = og;
   
    /*
    std::cout << "Binary representation of current with bit limit:" << std::endl;
      for (const auto& binary_value : og) {
        std::cout << binary_value << "\n";
      }
    */
  }
};

std::vector<std::string> MINE::previous;
int MINE::total_differing_bits = 0;
int MINE::total_bits_compared = 0;

}
#endif
