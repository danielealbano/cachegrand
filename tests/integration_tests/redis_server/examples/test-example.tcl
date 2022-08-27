start_server {tags {"example"}} {
    test {SET and GET example item} {
        r set x example
        r get x
    } {example}

    test {Command to skip because have forbidden tag} {
        r set x example
        r get x
    } {} {needs:repls}
}