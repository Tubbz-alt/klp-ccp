void ee_g(void)
{}

void ee_h(void)
{}

void* pu_f(void)
{
	return __builtin_choose_expr(0, ee_g, ee_h);
}
