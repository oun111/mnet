import java.io.IOException;
import java.net.InetSocketAddress;
import java.nio.ByteBuffer;
import java.nio.CharBuffer;
import java.nio.channels.ClosedChannelException;
import java.nio.channels.SelectionKey;
import java.nio.channels.Selector;
import java.nio.channels.ServerSocketChannel;
import java.nio.channels.SocketChannel;
import java.nio.charset.Charset;
import java.nio.charset.CharsetDecoder;
import java.util.Iterator;

/**
 * 
 */

/**
 * @author user1
 *
 */
public class NioSvr {
	
	
	public void start(int port) throws Exception {
		
		Selector selector = Selector.open(); 
		ServerSocketChannel serverSocketChannel = ServerSocketChannel.open(); 
		
		serverSocketChannel.socket().setReuseAddress(true); 
		serverSocketChannel.socket().bind(new InetSocketAddress(port)); 
		serverSocketChannel.configureBlocking(false);
		serverSocketChannel.register(selector, SelectionKey.OP_ACCEPT);
		
		System.out.println("server starts ok!");

		while (selector.select() > 0) {
			Iterator iterator = selector.selectedKeys().iterator();

			while (iterator.hasNext()) {
				SelectionKey key = null;
				
				try {
					key = (SelectionKey) iterator.next();
					iterator.remove(); 
					
					if (key.isAcceptable()) {
						ServerSocketChannel ssc = (ServerSocketChannel) key.channel(); 
						SocketChannel sc = ssc.accept(); 
						System.out .println("client addr: " + sc.socket().getRemoteSocketAddress());
						sc.configureBlocking(false); 
						ByteBuffer buffer = ByteBuffer.allocate(1024); 
						sc.register(selector, SelectionKey.OP_READ , buffer);
					}

					if (key.isReadable()) {
						rx(key) ;
					}

					if (key.isValid() && key.isWritable()) {
						tx(key);
					}
				} catch (IOException e) {
					e.printStackTrace();
					try {
						if (key != null) {
							key.cancel();
							key.channel().close();
						}
					} catch (ClosedChannelException cex) {
						e.printStackTrace();
					}
				}
			} // end while()
		}
		
		System.out.println("server exit...");
		
	}

	private void rx(SelectionKey key) throws Exception {
		
		ByteBuffer bb = ByteBuffer.allocate(1024);
		SocketChannel sc = (SocketChannel) key.channel();
		String content = "" ;
		int ret = 0 ;
		

		while ((ret=sc.read(bb))>0) {
			byte[] data = bb.array();
			String str  = new String(data);
			content += str ;
		}

		if (ret<0) {
			key.cancel();
			System.out.println("client disconnects");
		}
		else if (content.length()>0) {
			System.out.println("client request: " + content);
			
			bb = ByteBuffer.wrap(("server replys: " + content).getBytes());
			sc.write(bb);
		}
		
	}

	private void tx(SelectionKey key) {
		
	}

	/**
	 * @param args
	 * @throws Exception 
	 */
	public static void main(String[] args) throws Exception {
		NioSvr svr = new NioSvr();
		
		svr.start(8881);		
	}

}
