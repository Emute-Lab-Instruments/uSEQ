import { SerialVis } from './serialVis';
declare global {
    interface Window {
        serialVis?: SerialVis;
    }
}
