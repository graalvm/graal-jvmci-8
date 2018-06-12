public class Stress {
    public static void main(String[] args) throws Exception {
        for (int i = 0; i < 10_000_000; i++) {
            System.setProperty("jvmci.stress.init", "true");
        }
    }
}
