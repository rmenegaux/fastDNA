def make_model_name(args):
    if args.model_name:
        model_name = args.model_name
    else:
        model_name = 'fdna_k{}_d{}_e{}_lr{}'.format(
            args.k, args.d, args.e, args.lr)
        if args.noise > 0:
            model_name += '_n{}'.format(args.noise)
        if args.L != 200:
            model_name += '_L{}'.format(args.L)
        if args.skip != 1:
            model_name += '_skip{}'.format(int(args.skip))

    return model_name