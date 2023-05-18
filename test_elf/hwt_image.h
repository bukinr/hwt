struct hwt_image {
	char		*name;
	uintptr_t	entry;
	uintptr_t	vaddr;
	int		dynamic;
	char		*rtld_path;
};
