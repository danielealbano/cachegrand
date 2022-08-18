start_server {
    tags {"set"}
    overrides {
        "set-max-intset-entries" 512
    }
} {
    proc create_set {key entries} {
        r del $key
        foreach entry $entries { r sadd $key $entry }
    }

    test {SADD, SCARD, SISMEMBER, SMISMEMBER, SMEMBERS basics - regular set} {
        create_set myset {foo}
        assert_encoding hashtable myset
        assert_equal 1 [r sadd myset bar]
        assert_equal 0 [r sadd myset bar]
        assert_equal 2 [r scard myset]
        assert_equal 1 [r sismember myset foo]
        assert_equal 1 [r sismember myset bar]
        assert_equal 0 [r sismember myset bla]
        assert_equal {1} [r smismember myset foo]
        assert_equal {1 1} [r smismember myset foo bar]
        assert_equal {1 0} [r smismember myset foo bla]
        assert_equal {0 1} [r smismember myset bla foo]
        assert_equal {0} [r smismember myset bla]
        assert_equal {bar foo} [lsort [r smembers myset]]
    }
}