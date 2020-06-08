#ifndef CACHEGRAND_FIXTURES_HASHTABLE_H
#define CACHEGRAND_FIXTURES_HASHTABLE_H

#ifdef __cplusplus
namespace
{
#endif

// Fixtures
uintptr_t test_value_1 = 12345;
uintptr_t test_value_2 = 54321;

uint64_t buckets_initial_count_5 = 5;
uint64_t buckets_initial_count_100 = 100;
uint64_t buckets_initial_count_305 = 305;

uint64_t buckets_count_42 = 42;
uint64_t buckets_count_101 = 101;
uint64_t buckets_count_307 = 307;

hashtable_bucket_hash_t test_hash_zero = 0;

typedef struct test_key_same_bucket test_key_same_bucket_t;
struct test_key_same_bucket {
    const char* key;
    hashtable_key_size_t key_len;
    hashtable_bucket_hash_t key_hash;
    hashtable_bucket_hash_half_t key_hash_half;
};

test_key_same_bucket_t test_key_1_same_bucket[] = {
    { "test key 1_same_bucket_42_0036", 30, 0x3a1a6793c5984390, 0x3a1a6793 }, // match n. 1
    { "test key 1_same_bucket_42_0092", 30, 0x03887e20e6eadae0, 0x03887e20 }, // match n. 2
    { "test key 1_same_bucket_42_0118", 30, 0x1c51dba0c77bbe38, 0x1c51dba0 }, // match n. 3
    { "test key 1_same_bucket_42_0197", 30, 0xa369dfa4e169ab2c, 0xa369dfa4 }, // match n. 4
    { "test key 1_same_bucket_42_0201", 30, 0x57e289ab7575a73a, 0x57e289ab }, // match n. 5
    { "test key 1_same_bucket_42_0207", 30, 0xdbae2432f8ed753e, 0xdbae2432 }, // match n. 6
    { "test key 1_same_bucket_42_0218", 30, 0x0c0ed27de374bc7a, 0x0c0ed27d }, // match n. 7
    { "test key 1_same_bucket_42_0341", 30, 0x91a0132bf462567e, 0x91a0132b }, // match n. 8
    { "test key 1_same_bucket_42_0349", 30, 0xe56c946e6b9813b4, 0xe56c946e }, // match n. 9
    { "test key 1_same_bucket_42_0398", 30, 0xe41de88bd413c22a, 0xe41de88b }, // match n. 10
    { "test key 1_same_bucket_42_0429", 30, 0x4aee1521c64a17e0, 0x4aee1521 }, // match n. 11
    { "test key 1_same_bucket_42_0470", 30, 0x3277baf71196d346, 0x3277baf7 }, // match n. 12
    { "test key 1_same_bucket_42_0530", 30, 0xe66714bdf2aafcb4, 0xe66714bd }, // match n. 13
    { "test key 1_same_bucket_42_0556", 30, 0xebac069200629f06, 0xebac0692 }, // match n. 14
    { "test key 1_same_bucket_42_0557", 30, 0xe494be9a586d6d7e, 0xe494be9a }, // match n. 15
    { "test key 1_same_bucket_42_0561", 30, 0xcfc203062c2954c0, 0xcfc20306 }, // match n. 16
    { "test key 1_same_bucket_42_0579", 30, 0x288a94b8695698fe, 0x288a94b8 }, // match n. 17
    { "test key 1_same_bucket_42_0652", 30, 0xba123c897ad07e2e, 0xba123c89 }, // match n. 18
    { "test key 1_same_bucket_42_0663", 30, 0x390f6330d912acca, 0x390f6330 }, // match n. 19
    { "test key 1_same_bucket_42_0669", 30, 0xfc14fa325168f272, 0xfc14fa32 }, // match n. 20
    { "test key 1_same_bucket_42_0675", 30, 0xe16cf83ccb6ef70e, 0xe16cf83c }, // match n. 21
    { "test key 1_same_bucket_42_0703", 30, 0x36c204df63a1b2ec, 0x36c204df }, // match n. 22
    { "test key 1_same_bucket_42_0735", 30, 0x63674885494a58d2, 0x63674885 }, // match n. 23
    { "test key 1_same_bucket_42_0786", 30, 0xb4dc77d8134306fa, 0xb4dc77d8 }, // match n. 24
    { "test key 1_same_bucket_42_0809", 30, 0x371314bf61871190, 0x371314bf }, // match n. 25
    { "test key 1_same_bucket_42_0812", 30, 0xfed391c9d7c7d8b0, 0xfed391c9 }, // match n. 26
    { "test key 1_same_bucket_42_0825", 30, 0x871a8a4a93462110, 0x871a8a4a }, // match n. 27
    { "test key 1_same_bucket_42_0838", 30, 0x7ed2e1d2c689741a, 0x7ed2e1d2 }, // match n. 28
    { "test key 1_same_bucket_42_0939", 30, 0xdf88a26c1f32b242, 0xdf88a26c }, // match n. 29
    { "test key 1_same_bucket_42_1009", 30, 0x083d43c463d464dc, 0x083d43c4 }, // match n. 30
    { "test key 1_same_bucket_42_1039", 30, 0x4a90cd904d926bf6, 0x4a90cd90 }, // match n. 31
    { "test key 1_same_bucket_42_1059", 30, 0x57ec6c1ced8e9c92, 0x57ec6c1c }, // match n. 32
    { "test key 1_same_bucket_42_1093", 30, 0xb1eae6da57206360, 0xb1eae6da }, // match n. 33
    { "test key 1_same_bucket_42_1127", 30, 0xd527ccefb4fb895a, 0xd527ccef }, // match n. 34
    { "test key 1_same_bucket_42_1150", 30, 0x751a759b55b9c122, 0x751a759b }, // match n. 35
    { "test key 1_same_bucket_42_1170", 30, 0x4264af1a1a9cfe5e, 0x4264af1a }, // match n. 36
    { "test key 1_same_bucket_42_1174", 30, 0x89dca8e3ca664a70, 0x89dca8e3 }, // match n. 37
    { "test key 1_same_bucket_42_1176", 30, 0x5c41044f0e680faa, 0x5c41044f }, // match n. 38
    { "test key 1_same_bucket_42_1177", 30, 0x5675098f094ddf92, 0x5675098f }, // match n. 39
    { "test key 1_same_bucket_42_1178", 30, 0xe610482bbc844cbc, 0xe610482b }, // match n. 40
    { "test key 1_same_bucket_42_1197", 30, 0xc8f48a4d29e79e00, 0xc8f48a4d }, // match n. 41
    { "test key 1_same_bucket_42_1234", 30, 0x112c7d420e6c9652, 0x112c7d42 }, // match n. 42
    { "test key 1_same_bucket_42_1268", 30, 0xf49a376b254d9022, 0xf49a376b }, // match n. 43
    { "test key 1_same_bucket_42_1288", 30, 0xcaa0dcbc902f981e, 0xcaa0dcbc }, // match n. 44
    { "test key 1_same_bucket_42_1334", 30, 0x89e9b2abfafba132, 0x89e9b2ab }, // match n. 45
    { "test key 1_same_bucket_42_1337", 30, 0xb2bdb1110ef168dc, 0xb2bdb111 }, // match n. 46
    { "test key 1_same_bucket_42_1371", 30, 0x72328cbfa0d99ac0, 0x72328cbf }, // match n. 47
    { "test key 1_same_bucket_42_1391", 30, 0xb978bfb3df0fa1d8, 0xb978bfb3 }, // match n. 48
    { "test key 1_same_bucket_42_1398", 30, 0xbec19fe61bcb145e, 0xbec19fe6 }, // match n. 49
    { "test key 1_same_bucket_42_1432", 30, 0xa9ab186483da7ac4, 0xa9ab1864 }, // match n. 50
    { "test key 1_same_bucket_42_1434", 30, 0x3159e1d4f2464c90, 0x3159e1d4 }, // match n. 51
    { "test key 1_same_bucket_42_1472", 30, 0x7e3d1ebbd290069a, 0x7e3d1ebb }, // match n. 52
    { "test key 1_same_bucket_42_1482", 30, 0x2dd2cc3c267e8164, 0x2dd2cc3c }, // match n. 53
    { "test key 1_same_bucket_42_1527", 30, 0x83e275471435fb94, 0x83e27547 }, // match n. 54
    { "test key 1_same_bucket_42_1579", 30, 0x3f27f4b30d80a668, 0x3f27f4b3 }, // match n. 55
    { "test key 1_same_bucket_42_1652", 30, 0xc1736377881144ba, 0xc1736377 }, // match n. 56
    { "test key 1_same_bucket_42_1682", 30, 0x7d9d5ebf3d903296, 0x7d9d5ebf }, // match n. 57
    { "test key 1_same_bucket_42_1779", 30, 0xfc0b45fd9c8bac04, 0xfc0b45fd }, // match n. 58
    { "test key 1_same_bucket_42_1797", 30, 0x2e600b77db5b884a, 0x2e600b77 }, // match n. 59
    { "test key 1_same_bucket_42_1800", 30, 0x3d4f11c5987f53d0, 0x3d4f11c5 }, // match n. 60
    { "test key 1_same_bucket_42_1837", 30, 0xb676697ad9967ad6, 0xb676697a }, // match n. 61
    { "test key 1_same_bucket_42_1909", 30, 0x7e3c5663f3a66762, 0x7e3c5663 }, // match n. 62
    { "test key 1_same_bucket_42_1955", 30, 0x26abd0944b7ed214, 0x26abd094 }, // match n. 63
    { "test key 1_same_bucket_42_1978", 30, 0xa77098c5f2144114, 0xa77098c5 }, // match n. 64
    { "test key 1_same_bucket_42_2009", 30, 0xfa3a726e82a0a60e, 0xfa3a726e }, // match n. 65
    { "test key 1_same_bucket_42_2038", 30, 0xa64d3ff7e0434fb2, 0xa64d3ff7 }, // match n. 66
    { "test key 1_same_bucket_42_2115", 30, 0x0dc5937e73b59496, 0x0dc5937e }, // match n. 67
    { "test key 1_same_bucket_42_2137", 30, 0x4fdcfac4a1a597fe, 0x4fdcfac4 }, // match n. 68
    { "test key 1_same_bucket_42_2142", 30, 0xdc9a18584f18a57a, 0xdc9a1858 }, // match n. 69
    { "test key 1_same_bucket_42_2221", 30, 0x0e868db0e43234cc, 0x0e868db0 }, // match n. 70
    { "test key 1_same_bucket_42_2260", 30, 0x05e28bca3f5dccb8, 0x05e28bca }, // match n. 71
    { "test key 1_same_bucket_42_2262", 30, 0x4873440879ed40f2, 0x48734408 }, // match n. 72
    { "test key 1_same_bucket_42_2270", 30, 0x631e6cf6d980da3a, 0x631e6cf6 }, // match n. 73
    { "test key 1_same_bucket_42_2352", 30, 0x63f56a86103f771c, 0x63f56a86 }, // match n. 74
    { "test key 1_same_bucket_42_2365", 30, 0x6c53d5a32cdc5fd0, 0x6c53d5a3 }, // match n. 75
    { "test key 1_same_bucket_42_2433", 30, 0xa82af45f80610912, 0xa82af45f }, // match n. 76
    { "test key 1_same_bucket_42_2466", 30, 0x4627cd75271a6404, 0x4627cd75 }, // match n. 77
    { "test key 1_same_bucket_42_2530", 30, 0xf7c1b3350cd4c88c, 0xf7c1b335 }, // match n. 78
    { "test key 1_same_bucket_42_2543", 30, 0x70947fe7a6230678, 0x70947fe7 }, // match n. 79
    { "test key 1_same_bucket_42_2547", 30, 0x6867c41d603f9498, 0x6867c41d }, // match n. 80
    { "test key 1_same_bucket_42_2609", 30, 0x8e7f49421c9e4548, 0x8e7f4942 }, // match n. 81
    { "test key 1_same_bucket_42_2628", 30, 0x93e587bd8e901030, 0x93e587bd }, // match n. 82
    { "test key 1_same_bucket_42_2671", 30, 0xf22346a8cd414834, 0xf22346a8 }, // match n. 83
    { "test key 1_same_bucket_42_2691", 30, 0x9838479e84c37354, 0x9838479e }, // match n. 84
    { "test key 1_same_bucket_42_2786", 30, 0xae13090c840bc38a, 0xae13090c }, // match n. 85
    { "test key 1_same_bucket_42_2831", 30, 0xc6cbd7d8f7f6d7d4, 0xc6cbd7d8 }, // match n. 86
    { "test key 1_same_bucket_42_2902", 30, 0x45775bd3f7ed897a, 0x45775bd3 }, // match n. 87
    { "test key 1_same_bucket_42_2916", 30, 0x9ebd81bef7313eaa, 0x9ebd81be }, // match n. 88
    { "test key 1_same_bucket_42_2947", 30, 0x28d982e30dde1e90, 0x28d982e3 }, // match n. 89
    { "test key 1_same_bucket_42_2991", 30, 0x57ea021b2f700148, 0x57ea021b }, // match n. 90
    { "test key 1_same_bucket_42_2998", 30, 0x688739f10cafd5f2, 0x688739f1 }, // match n. 91
    { "test key 1_same_bucket_42_3028", 30, 0x170b5a3905ae667e, 0x170b5a39 }, // match n. 92
    { "test key 1_same_bucket_42_3062", 30, 0xe90d7215a92abb94, 0xe90d7215 }, // match n. 93
    { "test key 1_same_bucket_42_3066", 30, 0x4d4a7a6dc83c8502, 0x4d4a7a6d }, // match n. 94
    { "test key 1_same_bucket_42_3068", 30, 0xd7f2956cb6a94a5e, 0xd7f2956c }, // match n. 95
    { "test key 1_same_bucket_42_3175", 30, 0xe1874826cfd51fc0, 0xe1874826 }, // match n. 96
    { "test key 1_same_bucket_42_3322", 30, 0xd48297df9e89cd74, 0xd48297df }, // match n. 97
    { "test key 1_same_bucket_42_3455", 30, 0x67da907f23482652, 0x67da907f }, // match n. 98
    { "test key 1_same_bucket_42_3503", 30, 0xac59d0d37824d822, 0xac59d0d3 }, // match n. 99
    { "test key 1_same_bucket_42_3591", 30, 0x9e81536a1c14190c, 0x9e81536a }, // match n. 100
    { "test key 1_same_bucket_42_3657", 30, 0x5ddb5c8c676b4b68, 0x5ddb5c8c }, // match n. 101
    { "test key 1_same_bucket_42_3735", 30, 0x82468a7e834ef0bc, 0x82468a7e }, // match n. 102
    { "test key 1_same_bucket_42_3740", 30, 0x52af6557449d1c28, 0x52af6557 }, // match n. 103
    { "test key 1_same_bucket_42_3855", 30, 0x875625a56d0090c8, 0x875625a5 }, // match n. 104
    { "test key 1_same_bucket_42_3857", 30, 0xdabd8cefa92a8828, 0xdabd8cef }, // match n. 105
    { "test key 1_same_bucket_42_3860", 30, 0x8e03821b649fde2a, 0x8e03821b }, // match n. 106
    { "test key 1_same_bucket_42_3864", 30, 0xcd60db1a9a75762e, 0xcd60db1a }, // match n. 107
    { "test key 1_same_bucket_42_3878", 30, 0xa5da8e5fc06503f8, 0xa5da8e5f }, // match n. 108
    { "test key 1_same_bucket_42_3964", 30, 0x021802dbab5cc9ae, 0x021802db }, // match n. 109
    { "test key 1_same_bucket_42_3967", 30, 0xb0a219bcd15e60f4, 0xb0a219bc }, // match n. 110
    { "test key 1_same_bucket_42_4011", 30, 0xb7c03e3f3314550a, 0xb7c03e3f }, // match n. 111
    { "test key 1_same_bucket_42_4036", 30, 0x78f0b4211963f09e, 0x78f0b421 }, // match n. 112
    { "test key 1_same_bucket_42_4088", 30, 0xbe4c598835293fd8, 0xbe4c5988 }, // match n. 113
    { "test key 1_same_bucket_42_4166", 30, 0x589cc25decc1f81c, 0x589cc25d }, // match n. 114
    { "test key 1_same_bucket_42_4187", 30, 0x6cdea9d06b35cf96, 0x6cdea9d0 }, // match n. 115
    { "test key 1_same_bucket_42_4241", 30, 0xa07e4a55aa421a2e, 0xa07e4a55 }, // match n. 116
    { "test key 1_same_bucket_42_4249", 30, 0xba45e6302f0e79f2, 0xba45e630 }, // match n. 117
    { "test key 1_same_bucket_42_4250", 30, 0x088d4e824986b820, 0x088d4e82 }, // match n. 118
    { "test key 1_same_bucket_42_4257", 30, 0xb99f415883455d14, 0xb99f4158 }, // match n. 119
    { "test key 1_same_bucket_42_4303", 30, 0x8a89e3c675f809ba, 0x8a89e3c6 }, // match n. 120
    { "test key 1_same_bucket_42_4328", 30, 0xb3c5792413be87e0, 0xb3c57924 }, // match n. 121
    { "test key 1_same_bucket_42_4447", 30, 0x98587d4f82eef14e, 0x98587d4f }, // match n. 122
    { "test key 1_same_bucket_42_4474", 30, 0xfa8b5ad01771982a, 0xfa8b5ad0 }, // match n. 123
    { "test key 1_same_bucket_42_4501", 30, 0xaaf02bc36536acea, 0xaaf02bc3 }, // match n. 124
    { "test key 1_same_bucket_42_4546", 30, 0xc64951fedbcc79da, 0xc64951fe }, // match n. 125
    { "test key 1_same_bucket_42_4597", 30, 0x346026e5a15f8224, 0x346026e5 }, // match n. 126
    { "test key 1_same_bucket_42_4770", 30, 0x2cae5fb9ce164d6a, 0x2cae5fb9 }, // match n. 127
    { "test key 1_same_bucket_42_4786", 30, 0x98744cc848a7ccde, 0x98744cc8 }, // match n. 128
    { "test key 1_same_bucket_42_4800", 30, 0xd92c94d5affe37b2, 0xd92c94d5 }, // match n. 129
    { "test key 1_same_bucket_42_4805", 30, 0xa83716af50974440, 0xa83716af }, // match n. 130
    { "test key 1_same_bucket_42_4836", 30, 0xab0c5d045686fa00, 0xab0c5d04 }, // match n. 131
    { "test key 1_same_bucket_42_4906", 30, 0x7510e797fe6415dc, 0x7510e797 }, // match n. 132
    { "test key 1_same_bucket_42_4916", 30, 0xeaa978dd8402e2de, 0xeaa978dd }, // match n. 133
    { "test key 1_same_bucket_42_4974", 30, 0x0b154f4804ab11e4, 0x0b154f48 }, // match n. 134
    { "test key 1_same_bucket_42_5005", 30, 0x28404e5cae040af6, 0x28404e5c }, // match n. 135
    { "test key 1_same_bucket_42_5026", 30, 0x7cf02c6ff10fe252, 0x7cf02c6f }, // match n. 136
    { "test key 1_same_bucket_42_5156", 30, 0xb13d0ea28bc9984e, 0xb13d0ea2 }, // match n. 137
    { "test key 1_same_bucket_42_5198", 30, 0x2514e893f5f3c65a, 0x2514e893 }, // match n. 138
    { "test key 1_same_bucket_42_5286", 30, 0x7dee093293c52a14, 0x7dee0932 }, // match n. 139
    { "test key 1_same_bucket_42_5299", 30, 0x4451eff67a20a948, 0x4451eff6 }, // match n. 140
    { "test key 1_same_bucket_42_5306", 30, 0xfe89029bc8464c06, 0xfe89029b }, // match n. 141
    { "test key 1_same_bucket_42_5376", 30, 0x24823d8ccda19782, 0x24823d8c }, // match n. 142
    { "test key 1_same_bucket_42_5394", 30, 0x4297ce4a60de642a, 0x4297ce4a }, // match n. 143
    { "test key 1_same_bucket_42_5398", 30, 0x2432318719b1373a, 0x24323187 }, // match n. 144
    { "test key 1_same_bucket_42_5404", 30, 0x3a44739ee5f07dfc, 0x3a44739e }, // match n. 145
    { "test key 1_same_bucket_42_5408", 30, 0xb935905a4a4e3d86, 0xb935905a }, // match n. 146
    { "test key 1_same_bucket_42_5435", 30, 0x41200097b6586220, 0x41200097 }, // match n. 147
    { "test key 1_same_bucket_42_5553", 30, 0x1b710e33dfd88a2e, 0x1b710e33 }, // match n. 148
    { "test key 1_same_bucket_42_5565", 30, 0x2e756e2841487464, 0x2e756e28 }, // match n. 149
    { "test key 1_same_bucket_42_5572", 30, 0x350fae5f10e0ec24, 0x350fae5f }, // match n. 150
    { "test key 1_same_bucket_42_5581", 30, 0xb5ec1bdb839c03fa, 0xb5ec1bdb }, // match n. 151
    { "test key 1_same_bucket_42_5650", 30, 0x5816cb6b27310aca, 0x5816cb6b }, // match n. 152
    { "test key 1_same_bucket_42_5681", 30, 0xd1e9dca7b208f2de, 0xd1e9dca7 }, // match n. 153
    { "test key 1_same_bucket_42_5682", 30, 0xa440774c4ef4f440, 0xa440774c }, // match n. 154
    { "test key 1_same_bucket_42_5720", 30, 0x9802f10bccfb17dc, 0x9802f10b }, // match n. 155
    { "test key 1_same_bucket_42_5788", 30, 0xef84d4fee14cced2, 0xef84d4fe }, // match n. 156
    { "test key 1_same_bucket_42_5789", 30, 0x6267f82c788063de, 0x6267f82c }, // match n. 157
    { "test key 1_same_bucket_42_5829", 30, 0x3abc0e06526fd5cc, 0x3abc0e06 }, // match n. 158
    { "test key 1_same_bucket_42_5833", 30, 0x104882ceecc630b4, 0x104882ce }, // match n. 159
    { "test key 1_same_bucket_42_6026", 30, 0xb92b96c3ce7f8e44, 0xb92b96c3 }, // match n. 160
    { "test key 1_same_bucket_42_6041", 30, 0x5871b8dd1b6d9032, 0x5871b8dd }, // match n. 161
    { "test key 1_same_bucket_42_6170", 30, 0xada65798402805d2, 0xada65798 }, // match n. 162
    { "test key 1_same_bucket_42_6177", 30, 0x654ff623b9fb0e10, 0x654ff623 }, // match n. 163
    { "test key 1_same_bucket_42_6303", 30, 0x4915a9e853742084, 0x4915a9e8 }, // match n. 164
    { "test key 1_same_bucket_42_6319", 30, 0x5a653f81a90712ec, 0x5a653f81 }, // match n. 165
    { "test key 1_same_bucket_42_6335", 30, 0x7552b7772de6e8a2, 0x7552b777 }, // match n. 166
    { "test key 1_same_bucket_42_6342", 30, 0x83bc0866223506d2, 0x83bc0866 }, // match n. 167
    { "test key 1_same_bucket_42_6391", 30, 0xb15685aa49e819e8, 0xb15685aa }, // match n. 168
    { "test key 1_same_bucket_42_6393", 30, 0x8dd43b93edc3d4c6, 0x8dd43b93 }, // match n. 169
    { "test key 1_same_bucket_42_6403", 30, 0x245ec4fd4de0d4fc, 0x245ec4fd }, // match n. 170
    { "test key 1_same_bucket_42_6440", 30, 0x18be19c3d09fa752, 0x18be19c3 }, // match n. 171
    { "test key 1_same_bucket_42_6563", 30, 0xce809884c62332b6, 0xce809884 }, // match n. 172
    { "test key 1_same_bucket_42_6659", 30, 0xc7baa6b30fb73ae8, 0xc7baa6b3 }, // match n. 173
    { "test key 1_same_bucket_42_6685", 30, 0xd3121d1b7b1f67ac, 0xd3121d1b }, // match n. 174
    { "test key 1_same_bucket_42_6694", 30, 0x0b6f7f95895982bc, 0x0b6f7f95 }, // match n. 175
    { "test key 1_same_bucket_42_6732", 30, 0x490cf002d4a001e0, 0x490cf002 }, // match n. 176
    { "test key 1_same_bucket_42_6740", 30, 0x0f7cb2369f0a115e, 0x0f7cb236 }, // match n. 177
    { "test key 1_same_bucket_42_6796", 30, 0x05df172ae7e530e4, 0x05df172a }, // match n. 178
    { "test key 1_same_bucket_42_6819", 30, 0x551b0be797f26b8e, 0x551b0be7 }, // match n. 179
    { "test key 1_same_bucket_42_6827", 30, 0x436b58e458bfcfaa, 0x436b58e4 }, // match n. 180
    { "test key 1_same_bucket_42_6883", 30, 0x15851e6e6fefa4a4, 0x15851e6e }, // match n. 181
    { "test key 1_same_bucket_42_6928", 30, 0x97db0704530f8b72, 0x97db0704 }, // match n. 182
    { "test key 1_same_bucket_42_6942", 30, 0x2d5aabf1981cc938, 0x2d5aabf1 }, // match n. 183
    { "test key 1_same_bucket_42_6952", 30, 0x9c2a05d963e1e346, 0x9c2a05d9 }, // match n. 184
    { "test key 1_same_bucket_42_6964", 30, 0xa875c7617ce4e0a0, 0xa875c761 }, // match n. 185
    { "test key 1_same_bucket_42_7012", 30, 0x6ee4ea7c522c65ac, 0x6ee4ea7c }, // match n. 186
    { "test key 1_same_bucket_42_7054", 30, 0x71c2f7963007d1f4, 0x71c2f796 }, // match n. 187
    { "test key 1_same_bucket_42_7094", 30, 0xf7e885d4cd970164, 0xf7e885d4 }, // match n. 188
    { "test key 1_same_bucket_42_7103", 30, 0x26ed774639191728, 0x26ed7746 }, // match n. 189
    { "test key 1_same_bucket_42_7110", 30, 0x0585a60f544f74e0, 0x0585a60f }, // match n. 190
    { "test key 1_same_bucket_42_7137", 30, 0xe2e5de597ee17676, 0xe2e5de59 }, // match n. 191
    { "test key 1_same_bucket_42_7143", 30, 0x6b526d0d2cef81a8, 0x6b526d0d }, // match n. 192
    { "test key 1_same_bucket_42_7145", 30, 0x892e975ecb49e1d6, 0x892e975e }, // match n. 193
    { "test key 1_same_bucket_42_7155", 30, 0xaf1b5d46194d74a4, 0xaf1b5d46 }, // match n. 194
    { "test key 1_same_bucket_42_7186", 30, 0xee3c22d757012e12, 0xee3c22d7 }, // match n. 195
    { "test key 1_same_bucket_42_7197", 30, 0x0588ba2bde05f06c, 0x0588ba2b }, // match n. 196
    { "test key 1_same_bucket_42_7327", 30, 0x12a79defd4294392, 0x12a79def }, // match n. 197
    { "test key 1_same_bucket_42_7534", 30, 0xb11cdc6feecb12c4, 0xb11cdc6f }, // match n. 198
    { "test key 1_same_bucket_42_7541", 30, 0x1dcc14a5bbf4ef1c, 0x1dcc14a5 }, // match n. 199
    { "test key 1_same_bucket_42_7590", 30, 0xd661db081eaa1d70, 0xd661db08 }, // match n. 200
    { "test key 1_same_bucket_42_7619", 30, 0x288c11dc57ebfd94, 0x288c11dc }, // match n. 201
    { "test key 1_same_bucket_42_7635", 30, 0xda4cd17e649d57ec, 0xda4cd17e }, // match n. 202
    { "test key 1_same_bucket_42_7674", 30, 0xe178b1f9b2e9f6e4, 0xe178b1f9 }, // match n. 203
    { "test key 1_same_bucket_42_7685", 30, 0xc13f77297a6a7462, 0xc13f7729 }, // match n. 204
    { "test key 1_same_bucket_42_7717", 30, 0xf75559f344f7114e, 0xf75559f3 }, // match n. 205
    { "test key 1_same_bucket_42_7789", 30, 0x4fc6cae867d960dc, 0x4fc6cae8 }, // match n. 206
    { "test key 1_same_bucket_42_7875", 30, 0xde7cfc1c0735f4e4, 0xde7cfc1c }, // match n. 207
    { "test key 1_same_bucket_42_7928", 30, 0xf3f755fbea466d4e, 0xf3f755fb }, // match n. 208
    { "test key 1_same_bucket_42_7942", 30, 0x1ccab765df43868e, 0x1ccab765 }, // match n. 209
    { "test key 1_same_bucket_42_7958", 30, 0xc76dfc7084bfdfe4, 0xc76dfc70 }, // match n. 210
    { "test key 1_same_bucket_42_7963", 30, 0xaee1acdd71963dd8, 0xaee1acdd }, // match n. 211
    { "test key 1_same_bucket_42_7983", 30, 0x79b10f1be4633056, 0x79b10f1b }, // match n. 212
    { "test key 1_same_bucket_42_8009", 30, 0x21ee91382d173984, 0x21ee9138 }, // match n. 213
    { "test key 1_same_bucket_42_8017", 30, 0x82f7c52bd11b8b8e, 0x82f7c52b }, // match n. 214
    { "test key 1_same_bucket_42_8069", 30, 0xdfa93551bdcaba28, 0xdfa93551 }, // match n. 215
    { "test key 1_same_bucket_42_8117", 30, 0xf3c5ef715f02e522, 0xf3c5ef71 }, // match n. 216
    { "test key 1_same_bucket_42_8183", 30, 0x0646e965108b0fa4, 0x0646e965 }, // match n. 217
    { "test key 1_same_bucket_42_8360", 30, 0xdd15e7c56c6fd856, 0xdd15e7c5 }, // match n. 218
    { "test key 1_same_bucket_42_8409", 30, 0x6f1f0828dc1c327c, 0x6f1f0828 }, // match n. 219
    { "test key 1_same_bucket_42_8481", 30, 0xf1ecf23eb07c6b54, 0xf1ecf23e }, // match n. 220
    { "test key 1_same_bucket_42_8515", 30, 0x9b443f1ef0e83e02, 0x9b443f1e }, // match n. 221
    { "test key 1_same_bucket_42_8530", 30, 0x4d380d22c39e6bce, 0x4d380d22 }, // match n. 222
    { "test key 1_same_bucket_42_8539", 30, 0x6722104cadb388ea, 0x6722104c }, // match n. 223
    { "test key 1_same_bucket_42_8620", 30, 0xb09df9577805c7ba, 0xb09df957 }, // match n. 224
    { "test key 1_same_bucket_42_8694", 30, 0x382908a7c5106df6, 0x382908a7 }, // match n. 225
    { "test key 1_same_bucket_42_8793", 30, 0x467c5178a4357bea, 0x467c5178 }, // match n. 226
    { "test key 1_same_bucket_42_8898", 30, 0x4647f69ba4a95e1e, 0x4647f69b }, // match n. 227
    { "test key 1_same_bucket_42_8916", 30, 0x167ca3b13b8f2b22, 0x167ca3b1 }, // match n. 228
    { "test key 1_same_bucket_42_8949", 30, 0xc8825f4b4749912a, 0xc8825f4b }, // match n. 229
    { "test key 1_same_bucket_42_8960", 30, 0x80b8b8bb20020e52, 0x80b8b8bb }, // match n. 230
    { "test key 1_same_bucket_42_8989", 30, 0xf2efa6c32c08a0ce, 0xf2efa6c3 }, // match n. 231
    { "test key 1_same_bucket_42_9058", 30, 0xcbdb458d11393380, 0xcbdb458d }, // match n. 232
    { "test key 1_same_bucket_42_9110", 30, 0xe84234880905f400, 0xe8423488 }, // match n. 233
    { "test key 1_same_bucket_42_9152", 30, 0xd7bc5ff61aeddb52, 0xd7bc5ff6 }, // match n. 234
    { "test key 1_same_bucket_42_9159", 30, 0x9c434a0704123dc6, 0x9c434a07 }, // match n. 235
    { "test key 1_same_bucket_42_9194", 30, 0x2c487b264854c0a4, 0x2c487b26 }, // match n. 236
    { "test key 1_same_bucket_42_9228", 30, 0x3c7175cdef564b32, 0x3c7175cd }, // match n. 237
    { "test key 1_same_bucket_42_9260", 30, 0x324cc209dda24c88, 0x324cc209 }, // match n. 238
    { "test key 1_same_bucket_42_9282", 30, 0x689ce1c74084f452, 0x689ce1c7 }, // match n. 239
    { "test key 1_same_bucket_42_9395", 30, 0x0e2ca953ebc531f4, 0x0e2ca953 }, // match n. 240
    { "test key 1_same_bucket_42_9406", 30, 0x1b68e0d8d7097266, 0x1b68e0d8 }, // match n. 241
    { "test key 1_same_bucket_42_9481", 30, 0xc610f405ef4bdfc2, 0xc610f405 }, // match n. 242
    { "test key 1_same_bucket_42_9484", 30, 0x08f12973fc2340de, 0x08f12973 }, // match n. 243
    { "test key 1_same_bucket_42_9580", 30, 0xed4db3bbd6793680, 0xed4db3bb }, // match n. 244
    { "test key 1_same_bucket_42_9614", 30, 0x7eeab601a4683bae, 0x7eeab601 }, // match n. 245
    { "test key 1_same_bucket_42_9632", 30, 0xf7fd14ef9ecd8340, 0xf7fd14ef }, // match n. 246
    { "test key 1_same_bucket_42_9643", 30, 0xe687a07f4cbd68ce, 0xe687a07f }, // match n. 247
    { "test key 1_same_bucket_42_9649", 30, 0x99aad5de424a38b4, 0x99aad5de }, // match n. 248
    { "test key 1_same_bucket_42_9675", 30, 0x00e100c883b77ae4, 0x00e100c8 }, // match n. 249
    { "test key 1_same_bucket_42_9751", 30, 0x7e140dd809c446dc, 0x7e140dd8 }, // match n. 250
    { "test key 1_same_bucket_42_9755", 30, 0xea8308d9f156ed2e, 0xea8308d9 }, // match n. 251
    { "test key 1_same_bucket_42_9757", 30, 0x97c96d388e9c2c9e, 0x97c96d38 }, // match n. 252
    { "test key 1_same_bucket_42_9782", 30, 0xa420c017dcac4048, 0xa420c017 }, // match n. 253
    { "test key 1_same_bucket_42_9791", 30, 0x7f879cbc58b17496, 0x7f879cbc }, // match n. 254
    { "test key 1_same_bucket_42_9883", 30, 0x3f2d4707b7ff1ae8, 0x3f2d4707 }, // match n. 255
    { "test key 1_same_bucket_42_9903", 30, 0xf9396512fb82965e, 0xf9396512 }, // match n. 256
    { "test key 1_same_bucket_42_9949", 30, 0xd92b4cad8eeb27fe, 0xd92b4cad }, // match n. 257
    { "test key 1_same_bucket_42_9969", 30, 0xee86bedff8aa3ce4, 0xee86bedf }, // match n. 258
    { NULL, 0, 0, 0 }
};

char test_key_1[] = "test key 1";
hashtable_key_size_t test_key_1_len = 10;
hashtable_bucket_hash_t test_key_1_hash = (hashtable_bucket_hash_t)0xf1bdcc8aaccb614c;
hashtable_bucket_hash_half_t test_key_1_hash_half = test_key_1_hash >> 32u;
hashtable_bucket_index_t test_index_1_buckets_count_42 = test_key_1_hash % buckets_count_42;

char test_key_2[] = "test key 2";
hashtable_key_size_t test_key_2_len = 10;
hashtable_bucket_hash_t test_key_2_hash = (hashtable_bucket_hash_t)0x8c8b1b670da1324d;
hashtable_bucket_hash_half_t test_key_2_hash_half = test_key_2_hash >> 32u;
hashtable_bucket_index_t test_index_2_buckets_count_42 = test_key_2_hash % buckets_count_42;

#define HASHTABLE_DATA(buckets_count_v, ...) \
{ \
    hashtable_data_t* hashtable_data = hashtable_data_init(buckets_count_v); \
    __VA_ARGS__; \
    hashtable_data_free(hashtable_data); \
}

#define HASHTABLE_INIT(initial_size_v, can_auto_resize_v) \
    hashtable_config_t* hashtable_config = hashtable_config_init();  \
    hashtable_config->initial_size = initial_size_v; \
    hashtable_config->can_auto_resize = can_auto_resize_v; \
    \
    hashtable_t* hashtable = hashtable_init(hashtable_config); \

#define HASHTABLE_FREE() \
    hashtable_free(hashtable); \

#define HASHTABLE(initial_size_v, can_auto_resize_v, ...) \
{ \
    HASHTABLE_INIT(initial_size_v, can_auto_resize_v); \
    \
    __VA_ARGS__ \
    \
    HASHTABLE_FREE(); \
}

#define HASHTABLE_BUCKET_CHAIN_RING_NEW() \
({ \
    hashtable_bucket_chain_ring_t* var; \
    var = (hashtable_bucket_chain_ring_t*)xalloc_alloc(sizeof(hashtable_bucket_chain_ring_t)); \
    memset(var, 0, sizeof(hashtable_bucket_chain_ring_t)); \
    var; \
})

#define HASHTABLE_BUCKET_CHAIN_RING_SET_INDEX_SHARED(chain_ring, chain_ring_index, hash, value) \
    chain_ring->half_hashes[chain_ring_index] = hash >> 32u; \
    chain_ring->keys_values[chain_ring_index].data = value;

#define HASHTABLE_BUCKET_CHAIN_RING_SET_INDEX_KEY_INLINE(chain_ring, chain_ring_index, hash, key, key_size, value) \
    HASHTABLE_BUCKET_CHAIN_RING_SET_INDEX_SHARED(chain_ring, chain_ring_index, hash, value); \
    chain_ring->keys_values[chain_ring_index].flags = \
        HASHTABLE_BUCKET_KEY_VALUE_FLAG_FILLED | HASHTABLE_BUCKET_KEY_VALUE_FLAG_KEY_INLINE; \
    strncpy((char*)&chain_ring->keys_values[chain_ring_index].inline_key.data, key, HASHTABLE_KEY_INLINE_MAX_LENGTH);

#define HASHTABLE_BUCKET_CHAIN_RING_SET_INDEX_KEY_EXTERNAL(chain_ring, chain_ring_index, hash, key, key_size, value) \
    HASHTABLE_BUCKET_CHAIN_RING_SET_INDEX_SHARED(chain_ring, chain_ring_index, hash, value); \
    chain_ring->keys_values[chain_ring_index].flags = \
        HASHTABLE_BUCKET_KEY_VALUE_FLAG_FILLED; \
    chain_ring->keys_values[chain_ring_index].external_key.data = key; \
    chain_ring->keys_values[chain_ring_index].external_key.size = key_size; \
    chain_ring->keys_values[chain_ring_index].prefix_key.size = key_size; \
    strncpy((char*)&chain_ring->keys_values[chain_ring_index].prefix_key.data, key, HASHTABLE_KEY_PREFIX_SIZE);

#define HASHTABLE_BUCKET_NEW_KEY_INLINE(bucket_index, hash, key, key_size, value) \
    hashtable_bucket_chain_ring_t* chain_ring = HASHTABLE_BUCKET_CHAIN_RING_NEW(); \
    HASHTABLE_BUCKET_CHAIN_RING_SET_INDEX_KEY_INLINE(chain_ring, 1, hash, key, key_size, value); \
    hashtable->ht_current->buckets[bucket_index].chain_first_ring = chain_ring;

#define HASHTABLE_BUCKET_NEW_KEY_EXTERNAL(bucket_index, hash, key, key_size, value) \
    hashtable_bucket_chain_ring_t* chain_ring = HASHTABLE_BUCKET_CHAIN_RING_NEW(); \
    HASHTABLE_BUCKET_CHAIN_RING_SET_INDEX_KEY_EXTERNAL(chain_ring, 1, hash, key, key_size, value); \
    hashtable->ht_current->buckets[bucket_index].chain_first_ring = chain_ring;

#define HASHTABLE_BUCKET_NEW_CLEANUP(bucket_index) \
    HASHTABLE_BUCKET_CHAIN_RING_FREE(hashtable->ht_current->buckets[bucket_index]->chain_first_ring);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_FIXTURES_HASHTABLE_H
