export const defaultOptions = {
    width: 640,
    height: 240,
    sampleCount: 512,
    valueRange: { min: -1, max: 1 },
    initialValue: 0,
    line: {
        color: '#7cf0ff',
        width: 2,
        style: 'solid'
    },
    smoothing: {
        kind: 'moving-average',
        windowSize: 5
    },
    futureMask: {
        kind: 'dim',
        color: '#0f172a',
        opacity: 0.25
    },
    progressBar: {
        enabled: true,
        color: '#38bdf8',
        height: 6,
        backgroundColor: 'rgba(56, 189, 248, 0.1)'
    },
    containerBackground: 'transparent'
};
