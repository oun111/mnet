import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.ServerSocket;
import java.net.Socket;
import java.nio.ByteBuffer;



public class testServer {


    private Request parse_packet(byte data[]) throws Exception {
    	
    	Request req = new Request(data);

    	System.out.println("client sends: \n" +
    			           "short ver: " + req.getShortVer() + "\n" + 
    			           " version: " + req.getVersion()  + "\n" +
    			           " len: "     + req.getLen()  + "\n" +
    			           " msg: "     + new String(req.getMsg())  + "\n" +
    			           " magic: "   + req.getMagic() + "\n" + 
    			           " timestamp:"+ req.getTime());

    	return req ;
    }
    
    private byte[] rx_request(Socket socket, int pkt_size) throws IOException {
    	
        InputStream istr = socket.getInputStream();
        int len = 0, total = 0, rc = 0;
        byte data[] = new byte[1024];

        
        // read the head
        for (rc = pkt_size; (len=istr.read(data,0,rc))>0 && total<pkt_size; 
             total+=len, rc-=len) ;

        // XXX: this will close the client socket
        //istr.close();

        return data ;
    }
    
    private void tx_response(Request req, Socket socket) throws Exception {
    	
    	Response rsp = new Response() ;
        OutputStream ostr = socket.getOutputStream();
        String rspStr = "This is server response:\n";

        
      	rspStr += "short version: " + req.getShortVer() + "\r\n";
      	rspStr += "version: " + req.getVersion() + "\r\n";
      	rspStr += "length: " + req.getLen() + "\r\n";
      	rspStr += "message: " + new String(req.getMsg()) + "\r\n";
      	rspStr += "magic number: " + req.getMagic() + "\r\n";
      	rspStr += "timestamp: " + req.getTime() + "\r\n";
      	
      	rsp.setRspMsg(rspStr.getBytes());
      	rsp.setVersion(11);

        ostr.write(rsp.toBytes());
        socket.shutdownOutput();
        
        ostr.close();
    }
    

    @SuppressWarnings("resource")
	public static void main(String[] args) throws Exception   {
    	
    	ServerSocket serverSocket = null ;
        Socket socket = null ;
        testServer svr = null;

        try {
            serverSocket=new ServerSocket(8881);
            serverSocket.setReuseAddress(true);
            
            svr = new testServer() ;
            
        } catch (Exception e) {
            e.printStackTrace();
        }


        while (true) 
        {

          try {
            socket = serverSocket.accept();
            
            Request req = new Request();
            byte[] ba = svr.rx_request(socket,req.getTotalSize());
            
            req.toObject(ba) ;
            req.dump();

            svr.tx_response(req,socket);                

          }  catch (Exception e) {
            if (socket.isConnected())
              socket.close();

            e.printStackTrace();
          }

        }
        
    }

}

