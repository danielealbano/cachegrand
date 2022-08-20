start_server {tags {"example"}} {
    test {SET and GET example item} {
        r set x example
        r get x
    } {example}
}