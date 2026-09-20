module main
func main() {
    flow ptr void = malloc(i32)
    var mut_ref void = ptr
    mut_ref.(i32) = 100
    flow new_owner void = ptr
}
