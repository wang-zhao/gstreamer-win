
package ossbuild.extract;

/**
 * Exception regarding resource extraction and/or loading.
 * 
 * @author David Hoyt <dhoyt@hoytsoft.org>
 */
public class MissingResourceReferenceException extends RuntimeException {
    public MissingResourceReferenceException() {
    }

    public MissingResourceReferenceException(String message) {
        super(message);
    }

    public MissingResourceReferenceException(Throwable cause) {
        super(cause);
    }

    public MissingResourceReferenceException(String message, Throwable cause) {
        super(message, cause);
    }
}
