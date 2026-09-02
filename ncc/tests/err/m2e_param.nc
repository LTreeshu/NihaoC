module main

/* §12.3：const 实参传给 flow 参数 → 禁止（目标生命周期短于源） */
func consume(flow p void) {
    return
}

func main() {
    const a void = malloc(i32)
    consume(a)
    return
}
