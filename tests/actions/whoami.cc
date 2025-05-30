#include "../service_test_fixture.hpp"

class WhoamiTestFixture : public ServiceTestFixture
{
protected:
  boost::uuids::uuid received_uuid_{};
};

TEST_F(WhoamiTestFixture, OnSuccess)
{
  const auto _whoami_buffer = request_whoami_builder();

  const auto _response = send_and_receive(_whoami_buffer, 1 + 16);

  ASSERT_EQ(_response[0], std::byte{0x01});

  boost::uuids::uuid _uuid{};
  std::memcpy(_uuid.data, _response.data() + 1, 16);

  received_uuid_ = _uuid;

  bool _non_zero = false;
  for (const auto &b : _uuid)
    _non_zero |= (b != 0);
  ASSERT_TRUE(_non_zero);
}
