class NoneSmoother {
    constructor() {
        this.kind = 'none';
    }
    apply(input, output) {
        output.set(input);
    }
    configure() {
        // nothing to configure
    }
}
class MovingAverageSmoother {
    constructor() {
        this.kind = 'moving-average';
        this.window = 5;
    }
    apply(input, output) {
        const n = input.length;
        const window = Math.max(1, Math.min(this.window, n));
        let acc = 0;
        for (let i = 0; i < n; i++) {
            acc += input[i];
            if (i >= window) {
                acc -= input[i - window];
            }
            const divisor = i + 1 < window ? i + 1 : window;
            output[i] = acc / divisor;
        }
    }
    configure(options) {
        if (options.windowSize && Number.isFinite(options.windowSize)) {
            this.window = Math.max(1, Math.floor(options.windowSize));
        }
    }
}
class ExponentialSmoother {
    constructor() {
        this.kind = 'exponential';
        this.alpha = 0.25;
    }
    apply(input, output) {
        const n = input.length;
        if (n === 0) {
            return;
        }
        let prev = input[0];
        output[0] = prev;
        const alpha = this.alpha;
        const beta = 1 - alpha;
        for (let i = 1; i < n; i++) {
            prev = alpha * input[i] + beta * prev;
            output[i] = prev;
        }
    }
    configure(options) {
        if (options.alpha && Number.isFinite(options.alpha)) {
            const value = options.alpha;
            this.alpha = Math.min(1, Math.max(0.01, value));
        }
    }
}
class IirLowPassSmoother {
    constructor() {
        this.kind = 'iir-lowpass';
        this.cutoff = 0.1;
    }
    apply(input, output) {
        const n = input.length;
        if (n === 0) {
            return;
        }
        const rc = 1 / (this.cutoff * 2 * Math.PI);
        const dt = 1; // normalized sample period per tick
        const alpha = dt / (rc + dt);
        let prev = input[0];
        output[0] = prev;
        for (let i = 1; i < n; i++) {
            prev = prev + alpha * (input[i] - prev);
            output[i] = prev;
        }
    }
    configure(options) {
        if (options.cutoff && Number.isFinite(options.cutoff)) {
            this.cutoff = Math.max(0.001, Math.min(0.5, options.cutoff));
        }
        if (options.alpha && Number.isFinite(options.alpha)) {
            const alpha = Math.min(1, Math.max(0.001, options.alpha));
            const rc = (1 - alpha) / alpha;
            this.cutoff = 1 / (rc * 2 * Math.PI);
        }
    }
}
export class SmootherFactory {
    static create(options) {
        switch (options.kind) {
            case 'moving-average': {
                const ma = new MovingAverageSmoother();
                ma.configure(options);
                return ma;
            }
            case 'exponential': {
                const exp = new ExponentialSmoother();
                exp.configure(options);
                return exp;
            }
            case 'iir-lowpass': {
                const iir = new IirLowPassSmoother();
                iir.configure(options);
                return iir;
            }
            case 'none':
            default:
                return new NoneSmoother();
        }
    }
}
